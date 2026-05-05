#include "Database.h"
#include <windows.h>
#include <string>
#include <vector>
#include <cwctype>
#include <fstream>
#include <sstream>
#include <nlohmann/json.hpp>
#include "Helper.h"
#include "StringUtils.h"

using json = nlohmann::json;

// ---------------------------------------------------------------------------
// GLOBAL DB HANDLE
// ---------------------------------------------------------------------------

static sqlite3* g_db = nullptr;

// ---------------------------------------------------------------------------
// SQLITE STATEMENT RAII WRAPPER
// ---------------------------------------------------------------------------

class Statement
{
public:
    sqlite3_stmt* stmt = nullptr;

    ~Statement()
    {
        if (stmt)
            sqlite3_finalize(stmt);
    }
};

// ---------------------------------------------------------------------------
// TOKEN VALIDATION
// ---------------------------------------------------------------------------

bool IsValidToken(const std::wstring& token)
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
// TAG HELPERS
// ---------------------------------------------------------------------------

// Splits a comma-delimited wide string into a trimmed vector
static std::vector<std::wstring> SplitTagsW(const std::wstring& csv)
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

// Inserts tags for a given expansion id into emoji_tags
static void InsertTags(int expansionId, const std::vector<std::wstring>& tags)
{
    for (const auto& tag : tags)
    {
        if (tag.empty()) continue;

        Statement ts;
        if (sqlite3_prepare_v2(g_db,
            "INSERT INTO emoji_tags (expansion_id, tag) VALUES (?, ?);",
            -1, &ts.stmt, nullptr) != SQLITE_OK)
            continue;

        std::string tagUtf8 = WideToUtf8(tag);
        sqlite3_bind_int(ts.stmt, 1, expansionId);
        sqlite3_bind_text(ts.stmt, 2, tagUtf8.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_step(ts.stmt);
    }
}

// Deletes all tags for a given expansion id
static void DeleteTags(int expansionId)
{
    Statement ds;
    if (sqlite3_prepare_v2(g_db,
        "DELETE FROM emoji_tags WHERE expansion_id=?;",
        -1, &ds.stmt, nullptr) != SQLITE_OK)
        return;

    sqlite3_bind_int(ds.stmt, 1, expansionId);
    sqlite3_step(ds.stmt);
}

// ---------------------------------------------------------------------------
// SHARED ROW → Expansion PARSER
// Columns must be: 0=id, 1=token, 2=value, 3=type, 4=GROUP_CONCAT(tag)
// ---------------------------------------------------------------------------

static Expansion RowToExpansion(sqlite3_stmt* stmt)
{
    Expansion e;
    e.id = sqlite3_column_int(stmt, 0);
    e.token = Utf8ToWide((const char*)sqlite3_column_text(stmt, 1));
    e.value = Utf8ToWide((const char*)sqlite3_column_text(stmt, 2));
    e.type = Utf8ToWide((const char*)sqlite3_column_text(stmt, 3));

    const char* tagsRaw = (const char*)sqlite3_column_text(stmt, 4);
    if (tagsRaw && *tagsRaw)
        e.tags = SplitTagsW(Utf8ToWide(tagsRaw));

    return e;
}

// SQL shared by GetAll and Search – differs only in WHERE clause
static const char* kSelectWithTags =
"SELECT e.id, e.token, e.value, e.type, "
"       GROUP_CONCAT(t.tag, ',') "
"FROM   expansions e "
"LEFT JOIN emoji_tags t ON t.expansion_id = e.id ";

// ---------------------------------------------------------------------------
// OPEN DATABASE
// ---------------------------------------------------------------------------

bool DB_Open()
{
    std::wstring appDir = GetAppDataDir();
    std::wstring dbPathW = appDir + L"\\easyemoji.db";
    std::string  dbPath = WideToUtf8(dbPathW);

    if (sqlite3_open(dbPath.c_str(), &g_db) != SQLITE_OK)
    {
        sqlite3_close(g_db);
        g_db = nullptr;
        return false;
    }

    // Main expansions table
    const char* createExpansions =
        "CREATE TABLE IF NOT EXISTS expansions ("
        "  id         INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  token      TEXT    NOT NULL UNIQUE,"
        "  value      TEXT    NOT NULL,"
        "  type       TEXT    NOT NULL DEFAULT 'text',"
        "  created_at TEXT    DEFAULT (datetime('now'))"
        ");";

    // Emoji meta / tags tables
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

    // Key-value settings store
    const char* createSettings =
        "CREATE TABLE IF NOT EXISTS app_settings ("
        "  key   TEXT PRIMARY KEY,"
        "  value TEXT NOT NULL"
        ");";

    char* errMsg = nullptr;

    if (sqlite3_exec(g_db, createExpansions, nullptr, nullptr, &errMsg) != SQLITE_OK)
    {
        sqlite3_free(errMsg);
        sqlite3_close(g_db);
        g_db = nullptr;
        return false;
    }

    sqlite3_exec(g_db, createExtra, nullptr, nullptr, nullptr);
    sqlite3_exec(g_db, createSettings, nullptr, nullptr, nullptr);

    return true;
}

// ---------------------------------------------------------------------------
// CLOSE DATABASE
// ---------------------------------------------------------------------------

void DB_Close()
{
    if (g_db)
    {
        sqlite3_close(g_db);
        g_db = nullptr;
    }
}

// ---------------------------------------------------------------------------
// ADD EXPANSION  (with optional tags)
// ---------------------------------------------------------------------------

bool DB_AddExpansion(const std::wstring& token,
    const std::wstring& value,
    const std::wstring& type,
    const std::vector<std::wstring>& tags)
{
    if (!g_db || !IsValidToken(token) || value.empty())
        return false;

    const char* sql =
        "INSERT INTO expansions (token, value, type) VALUES (?, ?, ?);";

    Statement s;
    if (sqlite3_prepare_v2(g_db, sql, -1, &s.stmt, nullptr) != SQLITE_OK)
        return false;

    std::string tokenUtf8 = WideToUtf8(token);
    std::string valueUtf8 = WideToUtf8(value);
    std::string typeUtf8 = WideToUtf8(type);

    sqlite3_bind_text(s.stmt, 1, tokenUtf8.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(s.stmt, 2, valueUtf8.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(s.stmt, 3, typeUtf8.c_str(), -1, SQLITE_TRANSIENT);

    if (sqlite3_step(s.stmt) != SQLITE_DONE)
        return false;

    int id = (int)sqlite3_last_insert_rowid(g_db);
    InsertTags(id, tags);

    return true;
}

// ---------------------------------------------------------------------------
// UPDATE EXPANSION  (with optional tags)
// ---------------------------------------------------------------------------

bool DB_UpdateExpansion(int id,
    const std::wstring& token,
    const std::wstring& value,
    const std::wstring& type,
    const std::vector<std::wstring>& tags)
{
    if (!g_db || !IsValidToken(token) || value.empty())
        return false;

    const char* sql =
        "UPDATE expansions SET token=?, value=?, type=? WHERE id=?;";

    Statement s;
    if (sqlite3_prepare_v2(g_db, sql, -1, &s.stmt, nullptr) != SQLITE_OK)
        return false;

    std::string tokenUtf8 = WideToUtf8(token);
    std::string valueUtf8 = WideToUtf8(value);
    std::string typeUtf8 = WideToUtf8(type);

    sqlite3_bind_text(s.stmt, 1, tokenUtf8.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(s.stmt, 2, valueUtf8.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(s.stmt, 3, typeUtf8.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(s.stmt, 4, id);

    if (sqlite3_step(s.stmt) != SQLITE_DONE)
        return false;

    // Replace tags: delete old ones then re-insert
    DeleteTags(id);
    InsertTags(id, tags);

    return true;
}

// ---------------------------------------------------------------------------
// DELETE
// ---------------------------------------------------------------------------

bool DB_DeleteExpansion(int id)
{
    if (!g_db) return false;

    // Cascade-delete tags first (FK may not enforce ON DELETE CASCADE)
    DeleteTags(id);

    const char* sql = "DELETE FROM expansions WHERE id=?;";
    Statement s;
    if (sqlite3_prepare_v2(g_db, sql, -1, &s.stmt, nullptr) != SQLITE_OK)
        return false;

    sqlite3_bind_int(s.stmt, 1, id);
    return sqlite3_step(s.stmt) == SQLITE_DONE;
}

// ---------------------------------------------------------------------------
// GET ALL  (includes tags via LEFT JOIN + GROUP_CONCAT)
// ---------------------------------------------------------------------------

std::vector<Expansion> DB_GetAllExpansions()
{
    std::vector<Expansion> results;
    if (!g_db) return results;

    std::string sql =
        std::string(kSelectWithTags) +
        "GROUP BY e.id "
        "ORDER BY e.token ASC;";

    Statement s;
    if (sqlite3_prepare_v2(g_db, sql.c_str(), -1, &s.stmt, nullptr) != SQLITE_OK)
        return results;

    while (sqlite3_step(s.stmt) == SQLITE_ROW)
        results.push_back(RowToExpansion(s.stmt));

    return results;
}

// ---------------------------------------------------------------------------
// SEARCH  (prefix match on token; includes tags)
// ---------------------------------------------------------------------------

std::vector<Expansion> DB_Search(const std::wstring& query)
{
    std::vector<Expansion> results;
    if (!g_db) return results;

    std::string sql =
        std::string(kSelectWithTags) +
        "WHERE e.token LIKE ? "
        "   OR t.tag LIKE ? "
        "GROUP BY e.id "
        "ORDER BY "
        "   CASE WHEN e.token LIKE ? THEN 0 ELSE 1 END, "
        "   e.token ASC;";

    Statement s;
    if (sqlite3_prepare_v2(g_db, sql.c_str(), -1, &s.stmt, nullptr) != SQLITE_OK)
        return results;

    std::string q = WideToUtf8(query);
    std::string prefix = q + "%";
    std::string contains = "%" + q + "%";

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

// ---------------------------------------------------------------------------
// SETTINGS  (simple key-value store in app_settings table)
// ---------------------------------------------------------------------------

std::wstring DB_GetSetting(const std::wstring& key, const std::wstring& defaultVal)
{
    if (!g_db) return defaultVal;

    Statement s;
    if (sqlite3_prepare_v2(g_db,
        "SELECT value FROM app_settings WHERE key=?;",
        -1, &s.stmt, nullptr) != SQLITE_OK)
        return defaultVal;

    std::string keyUtf8 = WideToUtf8(key);
    sqlite3_bind_text(s.stmt, 1, keyUtf8.c_str(), -1, SQLITE_TRANSIENT);

    if (sqlite3_step(s.stmt) == SQLITE_ROW)
    {
        const char* val = (const char*)sqlite3_column_text(s.stmt, 0);
        if (val) return Utf8ToWide(val);
    }

    return defaultVal;
}

bool DB_SetSetting(const std::wstring& key, const std::wstring& value)
{
    if (!g_db) return false;

    Statement s;
    if (sqlite3_prepare_v2(g_db,
        "INSERT OR REPLACE INTO app_settings (key, value) VALUES (?, ?);",
        -1, &s.stmt, nullptr) != SQLITE_OK)
        return false;

    std::string keyUtf8 = WideToUtf8(key);
    std::string valueUtf8 = WideToUtf8(value);

    sqlite3_bind_text(s.stmt, 1, keyUtf8.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(s.stmt, 2, valueUtf8.c_str(), -1, SQLITE_TRANSIENT);

    return sqlite3_step(s.stmt) == SQLITE_DONE;
}

// ---------------------------------------------------------------------------
// DB_IsEmpty
// ---------------------------------------------------------------------------

bool DB_IsEmpty()
{
    const char* sql = "SELECT COUNT(*) FROM expansions;";
    sqlite3_stmt* stmt = nullptr;

    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, nullptr) != SQLITE_OK)
        return true;

    bool empty = true;
    if (sqlite3_step(stmt) == SQLITE_ROW)
        empty = (sqlite3_column_int(stmt, 0) == 0);

    sqlite3_finalize(stmt);
    return empty;
}

// ---------------------------------------------------------------------------
// SEED FROM JSON
// (unchanged – still inserts tags separately after DB_AddExpansion)
// ---------------------------------------------------------------------------

void SeedFromJson(const std::wstring& path)
{
    std::ifstream file(WideToUtf8(path));
    if (!file)
    {
        OutputDebugStringW((L"[ERROR] Failed to open JSON: " + path + L"\n").c_str());
        return;
    }

    json data;
    try { file >> data; }
    catch (const std::exception& e)
    {
        OutputDebugStringA(("[ERROR] JSON parse failed: " + std::string(e.what()) + "\n").c_str());
        return;
    }

    int inserted = 0;

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

                for (auto& c : token)
                    if (c == L' ') c = L'_';

                // Use updated DB_AddExpansion that inserts tags atomically
                if (!DB_AddExpansion(token, emoji, L"emoji", tagList))
                    continue;

                int id = (int)sqlite3_last_insert_rowid(g_db);

                // Insert category into emoji_meta
                if (!category.empty())
                {
                    sqlite3_stmt* stmt = nullptr;
                    sqlite3_prepare_v2(g_db,
                        "INSERT INTO emoji_meta (expansion_id, category) VALUES (?, ?);",
                        -1, &stmt, nullptr);

                    std::string cat = WideToUtf8(category);
                    sqlite3_bind_int(stmt, 1, id);
                    sqlite3_bind_text(stmt, 2, cat.c_str(), -1, SQLITE_TRANSIENT);
                    sqlite3_step(stmt);
                    sqlite3_finalize(stmt);
                }

                inserted++;
            }
        }
        catch (...) { continue; }
    }

    wchar_t dbg[128];
    swprintf(dbg, 128, L"[SEED DONE] Inserted: %d\n", inserted);
    OutputDebugStringW(dbg);
}