#include "Database.h"
#include "AppLog.h"
#include "Helper.h"
#include "StringUtils.h"

#include <windows.h>
#include <sqlite3.h>
#include <nlohmann/json.hpp>

#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <cwctype>

using json = nlohmann::json;

namespace {

    // ---------------------------------------------------------------------------
    // RAII statement wrapper
    // ---------------------------------------------------------------------------
    class Statement
    {
    public:
        sqlite3_stmt* stmt = nullptr;

        Statement() = default;
        Statement(const Statement&) = delete;
        Statement& operator=(const Statement&) = delete;

        Statement(Statement&& o) noexcept : stmt(o.stmt) { o.stmt = nullptr; }

        ~Statement() { reset(); }

        void reset()
        {
            if (stmt)
            {
                sqlite3_finalize(stmt);
                stmt = nullptr;
            }
        }

        bool prepare(sqlite3* db, const char* sql)
        {
            reset();
            int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
            if (rc != SQLITE_OK)
            {
                AppLog::Error(L"SQLite prepare failed: " + Utf8ToWide(sqlite3_errmsg(db)));
                return false;
            }
            return true;
        }

        int step() const { return sqlite3_step(stmt); }

        bool stepDone(sqlite3* db) const
        {
            int rc = sqlite3_step(stmt);
            if (rc != SQLITE_DONE)
            {
                AppLog::Error(L"SQLite step failed: " + Utf8ToWide(sqlite3_errmsg(db)));
                return false;
            }
            return true;
        }
    };

    // ---------------------------------------------------------------------------
    // RAII transaction helper
    // ---------------------------------------------------------------------------
    class Transaction
    {
    public:
        explicit Transaction(sqlite3* db) : m_db(db), m_committed(false)
        {
            if (!m_db) return;

            if (sqlite3_exec(m_db, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr) != SQLITE_OK)
            {
                AppLog::Error(L"SQLite BEGIN TRANSACTION failed");
                m_db = nullptr;
            }
        }

        ~Transaction()
        {
            if (m_db && !m_committed)
                rollback();
        }

        bool commit()
        {
            if (!m_db) return false;

            if (sqlite3_exec(m_db, "COMMIT;", nullptr, nullptr, nullptr) != SQLITE_OK)
            {
                AppLog::Error(L"SQLite COMMIT failed: " + Utf8ToWide(sqlite3_errmsg(m_db)));
                return false;
            }

            m_committed = true;
            return true;
        }

        void rollback()
        {
            if (!m_db) return;
            sqlite3_exec(m_db, "ROLLBACK;", nullptr, nullptr, nullptr);
            // Keep rollback as warn (it usually indicates some failure path).
            AppLog::Warn(L"Database transaction rolled back");
        }

        bool valid() const { return m_db != nullptr; }

    private:
        sqlite3* m_db;
        bool     m_committed;
    };

    // ---------------------------------------------------------------------------
    // DatabaseConnection – owns the sqlite3*
    // ---------------------------------------------------------------------------
    class DatabaseConnection
    {
    public:
        DatabaseConnection() = default;
        ~DatabaseConnection() { close(); }

        DatabaseConnection(const DatabaseConnection&) = delete;
        DatabaseConnection& operator=(const DatabaseConnection&) = delete;

        bool open(const std::wstring& dbPathW)
        {
            if (m_db)
            {
                AppLog::Warn(L"DB_Open called while database is already open");
                return true;
            }

            std::string dbPath = WideToUtf8(dbPathW);

            int rc = sqlite3_open(dbPath.c_str(), &m_db);
            if (rc != SQLITE_OK)
            {
                AppLog::Error(L"Failed to open database: " + dbPathW);
                sqlite3_close(m_db);
                m_db = nullptr;
                return false;
            }

            execPragmas();

            if (!createSchema())
            {
                AppLog::Error(L"Database schema creation failed");
                close();
                return false;
            }

            AppLog::Info(L"Database opened");
            return true;
        }

        void close()
        {
            if (m_db)
            {
                sqlite3_close(m_db);
                m_db = nullptr;
                AppLog::Info(L"Database closed");
            }
        }

        sqlite3* handle() const { return m_db; }

    private:
        sqlite3* m_db = nullptr;

        void execPragmas()
        {
            const char* pragmas =
                "PRAGMA journal_mode = WAL;"
                "PRAGMA foreign_keys = ON;"
                "PRAGMA synchronous  = NORMAL;"
                "PRAGMA temp_store   = MEMORY;";

            char* errMsg = nullptr;
            if (sqlite3_exec(m_db, pragmas, nullptr, nullptr, &errMsg) != SQLITE_OK)
            {
                if (errMsg)
                {
                    AppLog::Warn(L"SQLite PRAGMA warning: " + Utf8ToWide(errMsg));
                    sqlite3_free(errMsg);
                }
                else
                {
                    AppLog::Warn(L"SQLite PRAGMA warning");
                }
            }
        }

        bool createSchema()
        {
            const char* sql = R"SQL(
                CREATE TABLE IF NOT EXISTS expansions (
                    id         INTEGER PRIMARY KEY AUTOINCREMENT,
                    token      TEXT    NOT NULL UNIQUE COLLATE NOCASE,
                    value      TEXT    NOT NULL,
                    type       TEXT    NOT NULL DEFAULT 'text',
                    created_at TEXT    NOT NULL DEFAULT (datetime('now'))
                );

                CREATE TABLE IF NOT EXISTS emoji_meta (
                    id           INTEGER PRIMARY KEY AUTOINCREMENT,
                    expansion_id INTEGER NOT NULL,
                    category     TEXT,
                    FOREIGN KEY (expansion_id)
                        REFERENCES expansions(id) ON DELETE CASCADE
                );

                CREATE TABLE IF NOT EXISTS emoji_tags (
                    id           INTEGER PRIMARY KEY AUTOINCREMENT,
                    expansion_id INTEGER NOT NULL,
                    tag          TEXT    NOT NULL,
                    FOREIGN KEY (expansion_id)
                        REFERENCES expansions(id) ON DELETE CASCADE
                );

                CREATE TABLE IF NOT EXISTS app_settings (
                    key   TEXT PRIMARY KEY NOT NULL,
                    value TEXT NOT NULL
                );

                CREATE INDEX IF NOT EXISTS idx_expansions_token
                    ON expansions(token COLLATE NOCASE);

                CREATE INDEX IF NOT EXISTS idx_tags_expansion
                    ON emoji_tags(expansion_id);

                CREATE INDEX IF NOT EXISTS idx_tags_tag
                    ON emoji_tags(tag);
            )SQL";

            char* errMsg = nullptr;
            int rc = sqlite3_exec(m_db, sql, nullptr, nullptr, &errMsg);
            if (rc != SQLITE_OK)
            {
                if (errMsg)
                {
                    AppLog::Error(L"Schema creation failed: " + Utf8ToWide(errMsg));
                    sqlite3_free(errMsg);
                }
                else
                {
                    AppLog::Error(L"Schema creation failed");
                }
                return false;
            }

            return true;
        }
    };

    DatabaseConnection& GetConnection()
    {
        static DatabaseConnection conn;
        return conn;
    }

    inline sqlite3* DB() { return GetConnection().handle(); }

    // ---------------------------------------------------------------------------
    // Token validation
    // ---------------------------------------------------------------------------
    bool IsValidToken(const std::wstring& token)
    {
        if (token.empty() || token.size() > 50)
            return false;

        for (wchar_t c : token)
        {
            if (c == L' ' || c == L':' || c == L';')
                return false;

            bool allowed =
                iswalnum(c) ||
                c == L'_' || c == L'-' || c == L'.' ||
                c == L'/' || c == L'\\' ||
                c == L'(' || c == L')' ||
                c == L'[' || c == L']';

            if (!allowed)
                return false;
        }

        return true;
    }

    // ---------------------------------------------------------------------------
    // Tag helpers
    // ---------------------------------------------------------------------------
    std::vector<std::wstring> SplitTagsW(const std::wstring& csv)
    {
        std::vector<std::wstring> out;
        if (csv.empty()) return out;

        std::wstringstream ss(csv);
        std::wstring item;

        while (std::getline(ss, item, L','))
        {
            size_t s = item.find_first_not_of(L" \t");
            if (s == std::wstring::npos) continue;
            size_t e = item.find_last_not_of(L" \t");
            out.push_back(item.substr(s, e - s + 1));
        }

        return out;
    }

    bool InsertTags(sqlite3* db, int expansionId, const std::vector<std::wstring>& tags)
    {
        if (tags.empty()) return true;

        const char* sql = "INSERT INTO emoji_tags (expansion_id, tag) VALUES (?, ?);";

        for (const auto& tag : tags)
        {
            if (tag.empty()) continue;

            Statement ts;
            if (!ts.prepare(db, sql)) return false;

            std::string tagUtf8 = WideToUtf8(tag);
            sqlite3_bind_int(ts.stmt, 1, expansionId);
            sqlite3_bind_text(ts.stmt, 2, tagUtf8.c_str(), -1, SQLITE_TRANSIENT);

            if (!ts.stepDone(db))
                return false;
        }

        return true;
    }

    bool DeleteTags(sqlite3* db, int expansionId)
    {
        Statement ds;
        if (!ds.prepare(db, "DELETE FROM emoji_tags WHERE expansion_id=?;"))
            return false;

        sqlite3_bind_int(ds.stmt, 1, expansionId);
        return ds.stepDone(db);
    }

    // ---------------------------------------------------------------------------
    // Shared row parser
    // ---------------------------------------------------------------------------
    Expansion RowToExpansion(sqlite3_stmt* stmt)
    {
        Expansion e;
        e.id = sqlite3_column_int(stmt, 0);

        auto col = [&](int idx) -> std::wstring
            {
                const char* txt = reinterpret_cast<const char*>(sqlite3_column_text(stmt, idx));
                return txt ? Utf8ToWide(txt) : std::wstring{};
            };

        e.token = col(1);
        e.value = col(2);
        e.type = col(3);

        std::wstring tagsCsv = col(4);
        if (!tagsCsv.empty())
            e.tags = SplitTagsW(tagsCsv);

        return e;
    }

    constexpr const char* kSelectWithTags =
        "SELECT e.id, e.token, e.value, e.type, "
        "       GROUP_CONCAT(t.tag, ',') "
        "FROM   expansions e "
        "LEFT JOIN emoji_tags t ON t.expansion_id = e.id ";

} // namespace

// ===========================================================================
// PUBLIC API
// ===========================================================================

bool DB_Open()
{
    std::wstring appDir = GetAppDataDir();
    std::wstring dbPathW = appDir + L"\\easyemoji.db";

    // Keep this log minimal (and useful)
    AppLog::Info(L"Opening database...");
    return GetConnection().open(dbPathW);
}

void DB_Close()
{
    GetConnection().close();
}

bool DB_AddExpansion(const std::wstring& token,
    const std::wstring& value,
    const std::wstring& type,
    const std::vector<std::wstring>& tags)
{
    sqlite3* db = DB();
    if (!db)
    {
        AppLog::Error(L"DB_AddExpansion failed: database not open");
        return false;
    }

    if (!IsValidToken(token))
    {
        AppLog::Warn(L"Invalid token: " + token);
        return false;
    }

    if (value.empty())
    {
        AppLog::Warn(L"Cannot add expansion: value is empty");
        return false;
    }

    Transaction tx(db);
    if (!tx.valid()) return false;

    const char* sql = "INSERT INTO expansions (token, value, type) VALUES (?, ?, ?);";

    Statement s;
    if (!s.prepare(db, sql)) return false;

    std::string tokenUtf8 = WideToUtf8(token);
    std::string valueUtf8 = WideToUtf8(value);
    std::string typeUtf8 = WideToUtf8(type);

    sqlite3_bind_text(s.stmt, 1, tokenUtf8.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(s.stmt, 2, valueUtf8.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(s.stmt, 3, typeUtf8.c_str(), -1, SQLITE_TRANSIENT);

    if (!s.stepDone(db))
    {
        // Most common: UNIQUE constraint / duplicate token
        AppLog::Warn(L"Could not add expansion (maybe duplicate token): " + token);
        return false;
    }

    int newId = static_cast<int>(sqlite3_last_insert_rowid(db));

    if (!InsertTags(db, newId, tags))
    {
        AppLog::Error(L"Failed to add tags for: " + token);
        return false;
    }

    if (!tx.commit()) return false;

    AppLog::Info(L"Added: " + token);
    return true;
}

bool DB_UpdateExpansion(int id,
    const std::wstring& token,
    const std::wstring& value,
    const std::wstring& type,
    const std::vector<std::wstring>& tags)
{
    sqlite3* db = DB();
    if (!db)
    {
        AppLog::Error(L"DB_UpdateExpansion failed: database not open");
        return false;
    }

    if (!IsValidToken(token))
    {
        AppLog::Warn(L"Invalid token: " + token);
        return false;
    }

    if (value.empty())
    {
        AppLog::Warn(L"Cannot update expansion: value is empty");
        return false;
    }

    Transaction tx(db);
    if (!tx.valid()) return false;

    const char* sql = "UPDATE expansions SET token=?, value=?, type=? WHERE id=?;";

    Statement s;
    if (!s.prepare(db, sql)) return false;

    std::string tokenUtf8 = WideToUtf8(token);
    std::string valueUtf8 = WideToUtf8(value);
    std::string typeUtf8 = WideToUtf8(type);

    sqlite3_bind_text(s.stmt, 1, tokenUtf8.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(s.stmt, 2, valueUtf8.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(s.stmt, 3, typeUtf8.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(s.stmt, 4, id);

    if (!s.stepDone(db))
    {
        AppLog::Error(L"Update failed for id: " + std::to_wstring(id));
        return false;
    }

    if (sqlite3_changes(db) == 0)
    {
        AppLog::Warn(L"No expansion found to update (id): " + std::to_wstring(id));
        return false;
    }

    if (!DeleteTags(db, id) || !InsertTags(db, id, tags))
    {
        AppLog::Error(L"Failed to update tags for id: " + std::to_wstring(id));
        return false;
    }

    if (!tx.commit()) return false;

    AppLog::Info(L"Updated: " + token);
    return true;
}

bool DB_DeleteExpansion(int id)
{
    sqlite3* db = DB();
    if (!db)
    {
        AppLog::Error(L"DB_DeleteExpansion failed: database not open");
        return false;
    }

    Transaction tx(db);
    if (!tx.valid()) return false;

    // Safe even with ON DELETE CASCADE (kept for older DBs)
    if (!DeleteTags(db, id))
    {
        AppLog::Error(L"Failed to delete tags for id: " + std::to_wstring(id));
        return false;
    }

    const char* sql = "DELETE FROM expansions WHERE id=?;";
    Statement s;
    if (!s.prepare(db, sql)) return false;

    sqlite3_bind_int(s.stmt, 1, id);

    if (!s.stepDone(db))
    {
        AppLog::Error(L"Delete failed for id: " + std::to_wstring(id));
        return false;
    }

    if (sqlite3_changes(db) == 0)
    {
        AppLog::Warn(L"No expansion found to delete (id): " + std::to_wstring(id));
        return false;
    }

    if (!tx.commit()) return false;

    AppLog::Info(L"Deleted expansion id: " + std::to_wstring(id));
    return true;
}

std::vector<Expansion> DB_GetAllExpansions()
{
    std::vector<Expansion> results;
    sqlite3* db = DB();
    if (!db)
    {
        AppLog::Error(L"DB_GetAllExpansions failed: database not open");
        return results;
    }

    std::string sql =
        std::string(kSelectWithTags) +
        "GROUP BY e.id "
        "ORDER BY e.token ASC;";

    Statement s;
    if (!s.prepare(db, sql.c_str()))
        return results;

    while (s.step() == SQLITE_ROW)
        results.push_back(RowToExpansion(s.stmt));

    return results;
}

std::vector<Expansion> DB_Search(const std::wstring& query)
{
    std::vector<Expansion> results;
    sqlite3* db = DB();
    if (!db)
    {
        AppLog::Error(L"DB_Search failed: database not open");
        return results;
    }

    if (query.empty())
        return DB_GetAllExpansions();

    std::string sql =
        std::string(kSelectWithTags) +
        "WHERE e.token LIKE ? "
        "   OR t.tag   LIKE ? "
        "GROUP BY e.id "
        "ORDER BY "
        "   CASE WHEN e.token LIKE ? THEN 0 ELSE 1 END, "
        "   e.token ASC;";

    Statement s;
    if (!s.prepare(db, sql.c_str()))
        return results;

    std::string q = WideToUtf8(query);
    std::string prefix = q + "%";
    std::string contains = "%" + q + "%";

    sqlite3_bind_text(s.stmt, 1, prefix.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(s.stmt, 2, contains.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(s.stmt, 3, prefix.c_str(), -1, SQLITE_TRANSIENT);

    while (s.step() == SQLITE_ROW)
        results.push_back(RowToExpansion(s.stmt));

    return results;
}

std::wstring DB_GetSetting(const std::wstring& key, const std::wstring& defaultVal)
{
    sqlite3* db = DB();
    if (!db)
    {
        AppLog::Error(L"DB_GetSetting failed: database not open");
        return defaultVal;
    }

    Statement s;
    if (!s.prepare(db, "SELECT value FROM app_settings WHERE key=?;"))
        return defaultVal;

    std::string keyUtf8 = WideToUtf8(key);
    sqlite3_bind_text(s.stmt, 1, keyUtf8.c_str(), -1, SQLITE_TRANSIENT);

    if (s.step() == SQLITE_ROW)
    {
        const char* val = reinterpret_cast<const char*>(sqlite3_column_text(s.stmt, 0));
        if (val)
            return Utf8ToWide(val);
    }

    return defaultVal;
}

bool DB_SetSetting(const std::wstring& key, const std::wstring& value)
{
    sqlite3* db = DB();
    if (!db)
    {
        AppLog::Error(L"DB_SetSetting failed: database not open");
        return false;
    }

    if (key.empty())
    {
        AppLog::Warn(L"DB_SetSetting: key is empty");
        return false;
    }

    Statement s;
    if (!s.prepare(db,
        "INSERT OR REPLACE INTO app_settings (key, value) VALUES (?, ?);"))
        return false;

    std::string keyUtf8 = WideToUtf8(key);
    std::string valueUtf8 = WideToUtf8(value);

    sqlite3_bind_text(s.stmt, 1, keyUtf8.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(s.stmt, 2, valueUtf8.c_str(), -1, SQLITE_TRANSIENT);

    return s.stepDone(db);
}

bool DB_IsEmpty()
{
    sqlite3* db = DB();
    if (!db)
    {
        AppLog::Error(L"DB_IsEmpty failed: database not open");
        return true;
    }

    Statement s;
    if (!s.prepare(db, "SELECT COUNT(*) FROM expansions;"))
        return true;

    bool empty = true;
    if (s.step() == SQLITE_ROW)
        empty = (sqlite3_column_int(s.stmt, 0) == 0);

    return empty;
}

void SeedFromJson(const std::wstring& path)
{
    AppLog::Info(L"Seeding expansions...");

    std::ifstream file(WideToUtf8(path));
    if (!file.is_open())
    {
        AppLog::Error(L"Seed file not found: " + path);
        return;
    }

    json data;
    try
    {
        file >> data;
    }
    catch (...)
    {
        AppLog::Error(L"Failed to parse seed JSON");
        return;
    }

    if (!data.is_array())
    {
        AppLog::Error(L"Seed JSON root must be an array");
        return;
    }

    sqlite3* db = DB();
    if (!db)
    {
        AppLog::Error(L"Seed failed: database not open");
        return;
    }

    int inserted = 0;
    int skipped = 0;

    Transaction tx(db);
    if (!tx.valid()) return;

    for (const auto& item : data)
    {
        try
        {
            if (!item.contains("emoji") || !item.contains("aliases"))
            {
                ++skipped;
                continue;
            }

            std::wstring emoji = Utf8ToWide(item["emoji"].get<std::string>().c_str());

            std::wstring category;
            if (item.contains("category"))
                category = Utf8ToWide(item["category"].get<std::string>().c_str());

            std::vector<std::wstring> tagList;
            if (item.contains("tags") && item["tags"].is_array())
            {
                for (const auto& tagJson : item["tags"])
                    if (tagJson.is_string())
                        tagList.push_back(Utf8ToWide(tagJson.get<std::string>().c_str()));
            }

            for (const auto& aliasJson : item["aliases"])
            {
                if (!aliasJson.is_string()) continue;

                std::wstring token = Utf8ToWide(aliasJson.get<std::string>().c_str());
                for (auto& c : token) if (c == L' ') c = L'_';

                if (!IsValidToken(token))
                {
                    ++skipped;
                    continue;
                }

                const char* sqlExp =
                    "INSERT OR IGNORE INTO expansions (token, value, type) VALUES (?, ?, 'emoji');";

                Statement sExp;
                if (!sExp.prepare(db, sqlExp)) { ++skipped; continue; }

                std::string tokenUtf8 = WideToUtf8(token);
                std::string valueUtf8 = WideToUtf8(emoji);

                sqlite3_bind_text(sExp.stmt, 1, tokenUtf8.c_str(), -1, SQLITE_TRANSIENT);
                sqlite3_bind_text(sExp.stmt, 2, valueUtf8.c_str(), -1, SQLITE_TRANSIENT);

                // Not using stepDone here because IGNORE can lead to "done but no insert".
                sExp.step();

                int newId = static_cast<int>(sqlite3_last_insert_rowid(db));
                if (newId == 0)
                {
                    ++skipped;   // duplicate ignored
                    continue;
                }

                if (!InsertTags(db, newId, tagList))
                {
                    ++skipped;
                    continue;
                }

                if (!category.empty())
                {
                    const char* sqlMeta =
                        "INSERT INTO emoji_meta (expansion_id, category) VALUES (?, ?);";

                    Statement sMeta;
                    if (sMeta.prepare(db, sqlMeta))
                    {
                        std::string catUtf8 = WideToUtf8(category);
                        sqlite3_bind_int(sMeta.stmt, 1, newId);
                        sqlite3_bind_text(sMeta.stmt, 2, catUtf8.c_str(), -1, SQLITE_TRANSIENT);
                        sMeta.stepDone(db);
                    }
                }

                ++inserted;
            }
        }
        catch (...)
        {
            ++skipped;
        }
    }

    if (!tx.commit())
    {
        AppLog::Error(L"Seeding failed: commit failed");
        return;
    }

    AppLog::Info(L"Seeding complete. Inserted: " + std::to_wstring(inserted) +
        L", skipped: " + std::to_wstring(skipped));
}

void DB_Seed()
{
    if (!DB_IsEmpty())
        return;

    wchar_t exePath[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);

    std::wstring dir = exePath;
    size_t pos = dir.find_last_of(L"\\/");
    if (pos != std::wstring::npos)
        dir = dir.substr(0, pos);

    std::wstring jsonPath = dir + L"\\emoji.json";
    SeedFromJson(jsonPath);
}