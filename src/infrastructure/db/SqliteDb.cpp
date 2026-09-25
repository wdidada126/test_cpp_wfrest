#include "ecshop/infrastructure/db/SqliteDb.h"
#include "ecshop/SqliteSchema.h"

#include <sqlite3.h>

#include <filesystem>

namespace ecshop::infra {

static void check(int rc, sqlite3 *db, const char *what)
{
    if (rc == SQLITE_OK || rc == SQLITE_DONE || rc == SQLITE_ROW)
        return;
    throw DbError(std::string("sqlite ") + what + ": " + (db ? sqlite3_errmsg(db) : "unknown"));
}

SqliteDb::SqliteDb(const std::string &path)
{
    std::filesystem::path p(path);
    if (p.has_parent_path())
        std::filesystem::create_directories(p.parent_path());

    int rc = sqlite3_open_v2(path.c_str(), &db_,
                             SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX,
                             nullptr);
    check(rc, db_, "open");

    // Schema contains foreign keys; match the file's PRAGMA foreign_keys = ON.
    char *err = nullptr;
    sqlite3_exec(db_, "PRAGMA foreign_keys = ON;", nullptr, nullptr, &err);
    if (err)
        sqlite3_free(err);
}

SqliteDb::~SqliteDb()
{
    if (db_)
        sqlite3_close(db_);
}

void SqliteDb::ensureSchema()
{
    std::lock_guard<std::mutex> lock(mu_);

    sqlite3_stmt *stmt = nullptr;
    check(sqlite3_prepare_v2(db_, "SELECT COUNT(*) FROM sqlite_master WHERE type='table'",
                             -1, &stmt, nullptr),
          db_, "prepare");
    int rc = sqlite3_step(stmt);
    check(rc, db_, "step");
    int tables = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);
    if (tables > 0)
        return;

    char *err = nullptr;
    int src = sqlite3_exec(db_, kSqliteSchemaSql, nullptr, nullptr, &err);
    if (src != SQLITE_OK)
    {
        std::string msg = err ? err : "unknown";
        if (err)
            sqlite3_free(err);
        throw DbError("sqlite schema bootstrap: " + msg);
    }
}

static void bindAll(sqlite3_stmt *stmt, const Params &params)
{
    for (size_t i = 0; i < params.size(); ++i)
    {
        int rc = sqlite3_bind_text(stmt, static_cast<int>(i + 1),
                                   params[i].c_str(), static_cast<int>(params[i].size()),
                                   SQLITE_TRANSIENT);
        if (rc != SQLITE_OK)
            throw DbError("sqlite bind failed");
    }
}

std::vector<Row> SqliteDb::query(const std::string &sql, const Params &params)
{
    std::lock_guard<std::mutex> lock(mu_);

    sqlite3_stmt *stmt = nullptr;
    check(sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr), db_, "prepare");
    try
    {
        bindAll(stmt, params);

        std::vector<Row> rows;
        int rc;
        while ((rc = sqlite3_step(stmt)) == SQLITE_ROW)
        {
            Row row;
            int ncol = sqlite3_column_count(stmt);
            for (int i = 0; i < ncol; ++i)
            {
                const char *name = sqlite3_column_name(stmt, i);
                const unsigned char *text = sqlite3_column_text(stmt, i);
                row.set(name ? name : "", text ? reinterpret_cast<const char *>(text) : "");
            }
            rows.push_back(std::move(row));
        }
        check(rc, db_, "step");
        sqlite3_finalize(stmt);
        return rows;
    }
    catch (...)
    {
        sqlite3_finalize(stmt);
        throw;
    }
}

int64_t SqliteDb::execute(const std::string &sql, const Params &params)
{
    std::lock_guard<std::mutex> lock(mu_);

    sqlite3_stmt *stmt = nullptr;
    check(sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr), db_, "prepare");
    try
    {
        bindAll(stmt, params);
        check(sqlite3_step(stmt), db_, "step");
        int64_t changed = sqlite3_changes(db_);
        sqlite3_finalize(stmt);
        return changed;
    }
    catch (...)
    {
        sqlite3_finalize(stmt);
        throw;
    }
}

int64_t SqliteDb::lastInsertId()
{
    std::lock_guard<std::mutex> lock(mu_);
    return sqlite3_last_insert_rowid(db_);
}

} // namespace ecshop::infra
