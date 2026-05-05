#include "Database.h"
#include <windows.h>
#include <cwctype>
#include <fstream>
#include <sstream>
#include <nlohmann/json.hpp>
#include "Helper.h"
#include "StringUtils.h"

using json = nlohmann::json;

// ---------------------------------------------------------------------------
// Utf8Param Implementation
// ---------------------------------------------------------------------------

Utf8Param::Utf8Param(const std::wstring& ws)
    : data(WideToUtf8(ws))
{
}

const char* Utf8Param::c_str() const
{
    return data.c_str();
}

int Utf8Param::length() const
{
    return static_cast<int>(data.length());
}

// ---------------------------------------------------------------------------
// Database Implementation
// ---------------------------------------------------------------------------

Database::Database(const std::wstring& dbPath)
    : db(nullptr), dbPath(dbPath), lastError("")
{
}

Database::~Database()
{
    Close();
}

bool Database::Open()
{
    std::string dbPathUtf8 = WideToUtf8(dbPath);

    if (sqlite3_open(dbPathUtf8.c_str(), &db) != SQLITE_OK)
    {
        SetLastError("Failed to open database");
        if (db)
        {
            sqlite3_close(db);
            db = nullptr;
        }
        return false;
    }

    // Create tables
    const char* createExpansions =
        "CREATE TABLE IF NOT EXISTS expansions ("
        "  id         INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  token      TEXT    NOT NULL UNIQUE,"
        "  value      TEXT    NOT NULL,"
        "  type       TEXT    NOT NULL DEFAULT 'text',"
        "  created_at TEXT    DEFAULT (datetime('now'))"
        ");";

    const char* createExtra =
        "CREATE TABLE IF NOT EXISTS emoji_meta ("
        "  id           INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  expansion_id INTEGER,"
        "  category     TEXT,"
        "  FOREIGN KEY (expansion_id) REFERENCES expansions(id)"
        ");"

        "CREATE TABLE IF NOT EXISTS emoji_tags ("
        "  id           INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  expansion_id INTEGER,"
        "  tag          TEXT,"
        "  FOREIGN KEY (expansion_id) REFERENCES expansions(id)"
        ");";

    const char* createSettings =
        "CREATE TABLE IF NOT EXISTS app_settings ("
        "  key   TEXT PRIMARY KEY,"
        "  value TEXT NOT NULL"
        ");";

    // Create indexes for faster searches
    const char* createIndexes =
        "CREATE INDEX IF NOT EXISTS idx_token_prefix ON expansions(token);"
        "CREATE INDEX IF NOT EXISTS idx_expansion_tag ON emoji_tags(expansion_id);";

    char* errMsg = nullptr;

    if (sqlite3_exec(db, createExpansions, nullptr, nullptr, &errMsg) != SQLITE_OK)
    {
        SetLastError(errMsg ? errMsg : "Failed to create expansions table");
        sqlite3_free(errMsg);
        sqlite3_close(db);
        db = nullptr;
        return false;
    }

    sqlite3_exec(db, createExtra, nullptr, nullptr, nullptr);
    sqlite3_exec(db, createSettings, nullptr, nullptr, nullptr);
    sqlite3_exec(db, createIndexes, nullptr, nullptr, nullptr);

    return true;
}

void Database::Close()
{
    if (db)
    {
        sqlite3_close(db);
        db = nullptr;
    }
}

bool Database::IsOpen() const
{
    return db != nullptr;
}

Statement Database::Prepare(const char* sql)
{
    Statement s;
    if (!db)
    {
        SetLastError("Database not open");
        return s;
    }

    if (sqlite3_prepare_v2(db, sql, -1, &s.stmt, nullptr) != SQLITE_OK)
    {
        SetLastError(sqlite3_errmsg(db));
    }

    return s;
}

void Database::SetLastError(const std::string& error)
{
    lastError = error;
}

std::string Database::GetLastError() const
{
    return lastError;
}

DbError Database::SqliteToDbError(int sqliteCode)
{
    switch (sqliteCode)
    {
        case SQLITE_OK:
        case SQLITE_DONE:
            return DbError::OK;
        case SQLITE_CONSTRAINT:
            return DbError::CONSTRAINT_VIOLATION;
        case SQLITE_LOCKED:
            return DbError::DATABASE_LOCKED;
        case SQLITE_IOERR:
            return DbError::IO_ERROR;
        default:
            return DbError::UNKNOWN;
    }
}

// ---------------------------------------------------------------------------
// Token Validation
// ---------------------------------------------------------------------------

bool Database::IsValidToken(const std::wstring& token)
{
    if (token.empty() || token.size() > 50)
        return false;

    for (wchar_t c : token)
    {
        if (c == L' ' || c == L':' || c == L';')
            return false;

        if (!(iswalnum(c) ||
            c == L'_' || c == L'-' || c == L'.' ||
            c == L'/' || c == L'\\' ||
            c == L'(' || c == L')' ||
            c == L'[' || c == L']'))
        {
            return false;
        }
    }

    return true;
}

// ---------------------------------------------------------------------------
// Tag Helpers
// ---------------------------------------------------------------------------

std::vector<std::wstring> Database::SplitTags(const std::wstring& csv)
{
    std::vector<std::wstring> out;
    if (csv.empty()) return out;

    std::wstringstream ss(csv);
    std::wstring item;

    while (std::getline(ss, item, L','))
    {
        // trim whitespace
        size_t s = item.find_first_not_of(L" \t");
        if (s == std::wstring::npos) continue;
        size_t e = item.find_last_not_of(L" \t");
        out.push_back(item.substr(s, e - s + 1));
    }

    return out;
}

void Database::InsertTags(int expansionId, const std::vector<std::wstring>& tags)
{
    if (!db) return;

    for (const auto& tag : tags)
    {
        if (tag.empty()) continue;

        Statement s = Prepare("INSERT INTO emoji_tags (expansion_id, tag) VALUES (?, ?);");
        if (!s.IsValid())
            continue;

        Utf8Param tagUtf8(tag);
        sqlite3_bind_int(s.stmt, 1, expansionId);
        sqlite3_bind_text(s.stmt, 2, tagUtf8.c_str(), tagUtf8.length(), SQLITE_TRANSIENT);
        sqlite3_step(s.stmt);
    }
}

void Database::DeleteTags(int expansionId)
{
    if (!db) return;

    Statement s = Prepare("DELETE FROM emoji_tags WHERE expansion_id=?;");
    if (!s.IsValid())
        return;

    sqlite3_bind_int(s.stmt, 1, expansionId);
    sqlite3_step(s.stmt);
}

// ---------------------------------------------------------------------------
// Row Parser
// ---------------------------------------------------------------------------

Expansion Database::RowToExpansion(sqlite3_stmt* stmt, const RowIndex& idx)
{
    Expansion e;
    e.id = sqlite3_column_int(stmt, idx.id);
    e.token = Utf8ToWide((const char*)sqlite3_column_text(stmt, idx.token));
    e.value = Utf8ToWide((const char*)sqlite3_column_text(stmt, idx.value));
    e.type = Utf8ToWide((const char*)sqlite3_column_text(stmt, idx.type));

    const char* tagsRaw = (const char*)sqlite3_column_text(stmt, idx.tags);
    if (tagsRaw && *tagsRaw)
        e.tags = SplitTags(Utf8ToWide(tagsRaw));

    return e;
}

// ---------------------------------------------------------------------------
// CRUD Operations
// ---------------------------------------------------------------------------

DbError Database::AddExpansion(const std::wstring& token,
    const std::wstring& value,
    const std::wstring& type,
    const std::vector<std::wstring>& tags)
{
    if (!db)
    {
        SetLastError("Database not open");
        return DbError::NOT_OPEN;
    }

    if (!IsValidToken(token))
    {
        SetLastError("Invalid token");
        return DbError::CONSTRAINT_VIOLATION;
    }

    if (value.empty())
    {
        SetLastError("Value cannot be empty");
        return DbError::CONSTRAINT_VIOLATION;
    }

    const char* sql =
        "INSERT INTO expansions (token, value, type) VALUES (?, ?, ?);";

    Statement s = Prepare(sql);
    if (!s.IsValid())
        return DbError::PREPARE_ERROR;

    Utf8Param tokenUtf8(token);
    Utf8Param valueUtf8(value);
    Utf8Param typeUtf8(type);

    sqlite3_bind_text(s.stmt, 1, tokenUtf8.c_str(), tokenUtf8.length(), SQLITE_TRANSIENT);
    sqlite3_bind_text(s.stmt, 2, valueUtf8.c_str(), valueUtf8.length(), SQLITE_TRANSIENT);
    sqlite3_bind_text(s.stmt, 3, typeUtf8.c_str(), typeUtf8.length(), SQLITE_TRANSIENT);

    int result = sqlite3_step(s.stmt);
    if (result != SQLITE_DONE)
    {
        SetLastError(sqlite3_errmsg(db));
        return SqliteToDbError(result);
    }

    int id = (int)sqlite3_last_insert_rowid(db);
    InsertTags(id, tags);

    return DbError::OK;
}

DbError Database::UpdateExpansion(int id,
    const std::wstring& token,
    const std::wstring& value,
    const std::wstring& type,
    const std::vector<std::wstring>& tags)
{
    if (!db)
    {
        SetLastError("Database not open");
        return DbError::NOT_OPEN;
    }

    if (!IsValidToken(token))
    {
        SetLastError("Invalid token");
        return DbError::CONSTRAINT_VIOLATION;
    }

    if (value.empty())
    {
        SetLastError("Value cannot be empty");
        return DbError::CONSTRAINT_VIOLATION;
    }

    const char* sql =
        "UPDATE expansions SET token=?, value=?, type=? WHERE id=?;";

    Statement s = Prepare(sql);
    if (!s.IsValid())
        return DbError::PREPARE_ERROR;

    Utf8Param tokenUtf8(token);
    Utf8Param valueUtf8(value);
    Utf8Param typeUtf8(type);

    sqlite3_bind_text(s.stmt, 1, tokenUtf8.c_str(), tokenUtf8.length(), SQLITE_TRANSIENT);
    sqlite3_bind_text(s.stmt, 2, valueUtf8.c_str(), valueUtf8.length(), SQLITE_TRANSIENT);
    sqlite3_bind_text(s.stmt, 3, typeUtf8.c_str(), typeUtf8.length(), SQLITE_TRANSIENT);
    sqlite3_bind_int(s.stmt, 4, id);

    int result = sqlite3_step(s.stmt);
    if (result != SQLITE_DONE)
    {
        SetLastError(sqlite3_errmsg(db));
        return SqliteToDbError(result);
    }

    // Replace tags: delete old ones then re-insert
    DeleteTags(id);
    InsertTags(id, tags);

    return DbError::OK;
}

DbError Database::DeleteExpansion(int id)
{
    if (!db)
    {
        SetLastError("Database not open");
        return DbError::NOT_OPEN;
    }

    // Cascade-delete tags first (FK may not enforce ON DELETE CASCADE)
    DeleteTags(id);

    const char* sql = "DELETE FROM expansions WHERE id=?;";
    Statement s = Prepare(sql);
    if (!s.IsValid())
        return DbError::PREPARE_ERROR;

    sqlite3_bind_int(s.stmt, 1, id);
    int result = sqlite3_step(s.stmt);

    if (result != SQLITE_DONE)
    {
        SetLastError(sqlite3_errmsg(db));
        return SqliteToDbError(result);
    }

    return DbError::OK;
}

// ---------------------------------------------------------------------------
// Queries
// ---------------------------------------------------------------------------

std::vector<Expansion> Database::GetAllExpansions()
{
    std::vector<Expansion> results;
    if (!db)
    {
        SetLastError("Database not open");
        return results;
    }

    std::string sql =
        std::string(SELECT_WITH_TAGS) +
        "GROUP BY e.id "
        "ORDER BY e.token ASC;";

    Statement s = Prepare(sql.c_str());
    if (!s.IsValid())
        return results;

    while (sqlite3_step(s.stmt) == SQLITE_ROW)
        results.push_back(RowToExpansion(s.stmt));

    return results;
}

std::vector<Expansion> Database::Search(const std::wstring& query)
{
    std::vector<Expansion> results;
    if (!db)
    {
        SetLastError("Database not open");
        return results;
    }

    std::string sql =
        std::string(SELECT_WITH_TAGS) +
        "WHERE e.token LIKE ? "
        "   OR t.tag LIKE ? "
        "GROUP BY e.id "
        "ORDER BY "
        "   CASE WHEN e.token LIKE ? THEN 0 ELSE 1 END, "
        "   e.token ASC;";

    Statement s = Prepare(sql.c_str());
    if (!s.IsValid())
        return results;

    Utf8Param q(query);
    std::string prefix = std::string(q.c_str()) + "%";
    std::string contains = "%" + std::string(q.c_str()) + "%";

    // token prefix
    sqlite3_bind_text(s.stmt, 1, prefix.c_str(), -1, SQLITE_TRANSIENT);

    // tag match
    sqlite3_bind_text(s.stmt, 2, contains.c_str(), -1, SQLITE_TRANSIENT);

    // prioritize token matches
    sqlite3_bind_text(s.stmt, 3, prefix.c_str(), -1, SQLITE_TRANSIENT);

    while (sqlite3_step(s.stmt) == SQLITE_ROW)
        results.push_back(RowToExpansion(s.stmt));

    return results;
}

bool Database::IsEmpty()
{
    if (!db)
        return true;

    const char* sql = "SELECT COUNT(*) FROM expansions;";
    Statement s = Prepare(sql);
    if (!s.IsValid())
        return true;

    bool empty = true;
    if (sqlite3_step(s.stmt) == SQLITE_ROW)
        empty = (sqlite3_column_int(s.stmt, 0) == 0);

    return empty;
}

// ---------------------------------------------------------------------------
// Settings
// ---------------------------------------------------------------------------

std::wstring Database::GetSetting(const std::wstring& key, const std::wstring& defaultVal)
{
    if (!db)
        return defaultVal;

    Statement s = Prepare("SELECT value FROM app_settings WHERE key=?;");
    if (!s.IsValid())
        return defaultVal;

    Utf8Param keyUtf8(key);
    sqlite3_bind_text(s.stmt, 1, keyUtf8.c_str(), keyUtf8.length(), SQLITE_TRANSIENT);

    if (sqlite3_step(s.stmt) == SQLITE_ROW)
    {
        const char* val = (const char*)sqlite3_column_text(s.stmt, 0);
        if (val)
            return Utf8ToWide(val);
    }

    return defaultVal;
}

bool Database::SetSetting(const std::wstring& key, const std::wstring& value)
{
    if (!db)
        return false;

    Statement s = Prepare(
        "INSERT OR REPLACE INTO app_settings (key, value) VALUES (?, ?);");
    if (!s.IsValid())
        return false;

    Utf8Param keyUtf8(key);
    Utf8Param valueUtf8(value);

    sqlite3_bind_text(s.stmt, 1, keyUtf8.c_str(), keyUtf8.length(), SQLITE_TRANSIENT);
    sqlite3_bind_text(s.stmt, 2, valueUtf8.c_str(), valueUtf8.length(), SQLITE_TRANSIENT);

    return sqlite3_step(s.stmt) == SQLITE_DONE;
}

// ---------------------------------------------------------------------------
// Transactions
// ---------------------------------------------------------------------------

DbError Database::BeginTransaction()
{
    if (!db)
        return DbError::NOT_OPEN;

    int result = sqlite3_exec(db, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr);
    if (result != SQLITE_OK)
    {
        SetLastError(sqlite3_errmsg(db));
        return SqliteToDbError(result);
    }
    return DbError::OK;
}

DbError Database::CommitTransaction()
{
    if (!db)
        return DbError::NOT_OPEN;

    int result = sqlite3_exec(db, "COMMIT;", nullptr, nullptr, nullptr);
    if (result != SQLITE_OK)
    {
        SetLastError(sqlite3_errmsg(db));
        return SqliteToDbError(result);
    }
    return DbError::OK;
}

DbError Database::RollbackTransaction()
{
    if (!db)
        return DbError::NOT_OPEN;

    int result = sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
    if (result != SQLITE_OK)
    {
        SetLastError(sqlite3_errmsg(db));
        return SqliteToDbError(result);
    }
    return DbError::OK;
}

// ---------------------------------------------------------------------------
// Seeding from JSON
// ---------------------------------------------------------------------------

DbError Database::SeedFromJson(const std::wstring& path)
{
    std::ifstream file(WideToUtf8(path));
    if (!file)
    {
        SetLastError("Failed to open JSON file: " + WideToUtf8(path));
        OutputDebugStringW((L"[ERROR] Failed to open JSON: " + path + L"\n").c_str());
        return DbError::IO_ERROR;
    }

    json data;
    try
    {
        file >> data;
    }
    catch (const std::exception& e)
    {
        SetLastError(std::string("JSON parse failed: ") + e.what());
        OutputDebugStringA(("[ERROR] JSON parse failed: " + std::string(e.what()) + "\n").c_str());
        return DbError::IO_ERROR;
    }

    // Use transaction for bulk insert performance
    BeginTransaction();

    int inserted = 0;
    int failed = 0;

    for (const auto& item : data)
    {
        try
        {
            if (!item.contains("emoji") || !item.contains("aliases"))
                continue;

            std::wstring emoji = Utf8ToWide(item["emoji"].get<std::string>().c_str());

            std::wstring category;
            if (item.contains("category"))
                category = Utf8ToWide(item["category"].get<std::string>().c_str());

            // Build tags list from "tags" array in JSON
            std::vector<std::wstring> tagList;
            if (item.contains("tags"))
            {
                for (const auto& tagJson : item["tags"])
                    tagList.push_back(Utf8ToWide(tagJson.get<std::string>().c_str()));
            }

            for (const auto& aliasJson : item["aliases"])
            {
                std::wstring token = Utf8ToWide(aliasJson.get<std::string>().c_str());

                // Replace spaces with underscores
                for (auto& c : token)
                    if (c == L' ') c = L'_';

                DbError err = AddExpansion(token, emoji, L"emoji", tagList);
                if (err != DbError::OK)
                {
                    failed++;
                    continue;
                }

                int id = (int)sqlite3_last_insert_rowid(db);

                // Insert category into emoji_meta
                if (!category.empty())
                {
                    Statement stmt = Prepare(
                        "INSERT INTO emoji_meta (expansion_id, category) VALUES (?, ?);");
                    if (stmt.IsValid())
                    {
                        Utf8Param catUtf8(category);
                        sqlite3_bind_int(stmt.stmt, 1, id);
                        sqlite3_bind_text(stmt.stmt, 2, catUtf8.c_str(), catUtf8.length(), SQLITE_TRANSIENT);
                        sqlite3_step(stmt.stmt);
                    }
                }

                inserted++;
            }
        }
        catch (const std::exception& e)
        {
            failed++;
            continue;
        }
    }

    CommitTransaction();

    wchar_t dbg[256];
    swprintf_s(dbg, 256, L"[SEED DONE] Inserted: %d, Failed: %d\n", inserted, failed);
    OutputDebugStringW(dbg);

    return DbError::OK;
}
