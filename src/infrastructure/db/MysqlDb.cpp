#include "ecshop/infrastructure/db/MysqlDb.h"

#include <mysql.h>

#include <cstring>
#include <memory>
#include <vector>

namespace ecshop::infra {

struct StmtGuard
{
    MYSQL_STMT *stmt;
    ~StmtGuard()
    {
        if (stmt)
            mysql_stmt_close(stmt);
    }
};

MysqlDb::MysqlDb(const Options &opts)
{
    conn_ = mysql_init(nullptr);
    if (!conn_)
        throw DbError("mysql_init failed");

    unsigned int timeout = 5;
    mysql_options(conn_, MYSQL_OPT_CONNECT_TIMEOUT, &timeout);

    if (!mysql_real_connect(conn_, opts.host.c_str(), opts.user.c_str(),
                            opts.password.c_str(), opts.database.c_str(),
                            static_cast<unsigned int>(opts.port), nullptr, 0))
    {
        std::string msg = mysql_error(conn_);
        mysql_close(conn_);
        conn_ = nullptr;
        throw DbError("mysql connect failed: " + msg);
    }

    mysql_set_character_set(conn_, "utf8mb4");
}

MysqlDb::~MysqlDb()
{
    if (conn_)
        mysql_close(conn_);
}

static void throwStmt(MYSQL_STMT *stmt, const char *what)
{
    throw DbError(std::string("mysql ") + what + ": " + mysql_stmt_error(stmt));
}

std::vector<Row> MysqlDb::query(const std::string &sql, const Params &params)
{
    std::lock_guard<std::mutex> lock(mu_);

    StmtGuard guard{mysql_stmt_init(conn_)};
    if (!guard.stmt)
        throw DbError("mysql_stmt_init failed");

    if (mysql_stmt_prepare(guard.stmt, sql.c_str(), static_cast<unsigned long>(sql.size())) != 0)
        throwStmt(guard.stmt, "prepare");

    std::vector<unsigned long> lens(params.size());
    std::unique_ptr<bool[]> nulls(params.empty() ? nullptr : new bool[params.size()]());
    std::vector<MYSQL_BIND> binds(params.size());
    std::memset(binds.data(), 0, binds.size() * sizeof(MYSQL_BIND));
    for (size_t i = 0; i < params.size(); ++i)
    {
        lens[i] = static_cast<unsigned long>(params[i].size());
        binds[i].buffer_type = MYSQL_TYPE_STRING;
        binds[i].buffer = const_cast<char *>(params[i].c_str());
        binds[i].buffer_length = lens[i];
        binds[i].length = &lens[i];
        binds[i].is_null = &nulls[i];
    }
    if (!binds.empty() && mysql_stmt_bind_param(guard.stmt, binds.data()) != 0)
        throwStmt(guard.stmt, "bind param");

    if (mysql_stmt_execute(guard.stmt) != 0)
        throwStmt(guard.stmt, "execute");

    MYSQL_RES *meta = mysql_stmt_result_metadata(guard.stmt);
    if (!meta)
        return {};

    unsigned int ncol = mysql_num_fields(meta);
    MYSQL_FIELD *fields = mysql_fetch_fields(meta);

    std::vector<std::string> names(ncol);
    std::vector<std::string> values(ncol, std::string(4096, '\0'));
    std::vector<unsigned long> vlens(ncol, 0);
    std::unique_ptr<bool[]> vnulls(new bool[ncol]());
    std::vector<MYSQL_BIND> rbinds(ncol);
    std::memset(rbinds.data(), 0, rbinds.size() * sizeof(MYSQL_BIND));
    for (unsigned int i = 0; i < ncol; ++i)
    {
        names[i] = fields[i].name;
        rbinds[i].buffer_type = MYSQL_TYPE_STRING;
        rbinds[i].buffer = values[i].data();
        rbinds[i].buffer_length = static_cast<unsigned long>(values[i].size());
        rbinds[i].length = &vlens[i];
        rbinds[i].is_null = &vnulls[i];
    }
    if (mysql_stmt_bind_result(guard.stmt, rbinds.data()) != 0)
    {
        mysql_free_result(meta);
        throwStmt(guard.stmt, "bind result");
    }

    if (mysql_stmt_store_result(guard.stmt) != 0)
    {
        mysql_free_result(meta);
        throwStmt(guard.stmt, "store result");
    }

    std::vector<Row> rows;
    for (int rc = mysql_stmt_fetch(guard.stmt); rc == 0 || rc == MYSQL_DATA_TRUNCATED;
         rc = mysql_stmt_fetch(guard.stmt))
    {
        Row row;
        for (unsigned int i = 0; i < ncol; ++i)
        {
            if (vnulls[i])
            {
                row.set(names[i], "");
                continue;
            }
            if (rc == MYSQL_DATA_TRUNCATED && vlens[i] > values[i].size())
            {
                std::string big(vlens[i], '\0');
                MYSQL_BIND one{};
                one.buffer_type = MYSQL_TYPE_STRING;
                one.buffer = big.data();
                one.buffer_length = static_cast<unsigned long>(big.size());
                one.length = &vlens[i];
                one.is_null = &vnulls[i];
                if (mysql_stmt_fetch_column(guard.stmt, &one, i, 0) == 0)
                    row.set(names[i], big);
                else
                    row.set(names[i], values[i].substr(0, vlens[i]));
            }
            else
            {
                row.set(names[i], values[i].substr(0, vlens[i]));
            }
        }
        rows.push_back(std::move(row));
    }

    mysql_free_result(meta);
    return rows;
}

int64_t MysqlDb::execute(const std::string &sql, const Params &params)
{
    std::lock_guard<std::mutex> lock(mu_);

    StmtGuard guard{mysql_stmt_init(conn_)};
    if (!guard.stmt)
        throw DbError("mysql_stmt_init failed");

    if (mysql_stmt_prepare(guard.stmt, sql.c_str(), static_cast<unsigned long>(sql.size())) != 0)
        throwStmt(guard.stmt, "prepare");

    std::vector<unsigned long> lens(params.size());
    std::unique_ptr<bool[]> nulls(params.empty() ? nullptr : new bool[params.size()]());
    std::vector<MYSQL_BIND> binds(params.size());
    std::memset(binds.data(), 0, binds.size() * sizeof(MYSQL_BIND));
    for (size_t i = 0; i < params.size(); ++i)
    {
        lens[i] = static_cast<unsigned long>(params[i].size());
        binds[i].buffer_type = MYSQL_TYPE_STRING;
        binds[i].buffer = const_cast<char *>(params[i].c_str());
        binds[i].buffer_length = lens[i];
        binds[i].length = &lens[i];
        binds[i].is_null = &nulls[i];
    }
    if (!binds.empty() && mysql_stmt_bind_param(guard.stmt, binds.data()) != 0)
        throwStmt(guard.stmt, "bind param");

    if (mysql_stmt_execute(guard.stmt) != 0)
        throwStmt(guard.stmt, "execute");

    last_insert_id_ = static_cast<int64_t>(mysql_stmt_insert_id(guard.stmt));
    return static_cast<int64_t>(mysql_stmt_affected_rows(guard.stmt));
}

int64_t MysqlDb::lastInsertId()
{
    std::lock_guard<std::mutex> lock(mu_);
    return last_insert_id_;
}

} // namespace ecshop::infra
