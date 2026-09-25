#include "ecshop/infrastructure/SqlAdminRepository.h"
#include "ecshop/shared/TimeUtil.h"

#include <cstdlib>
#include <vector>

namespace ecshop::infra {

// Tables sit outside docs/sql/001_core_schema (docs allow additive,
// non-destructive upgrades via CREATE TABLE IF NOT EXISTS).
void SqlAdminRepository::ensureSchema()
{
    db_->execute("CREATE TABLE IF NOT EXISTS " + db_->table("admin_user") + " ("
                 " admin_id INTEGER PRIMARY KEY AUTOINCREMENT,"
                 " user_name VARCHAR(60) NOT NULL UNIQUE,"
                 " password_hash VARCHAR(255) NOT NULL,"
                 " role VARCHAR(20) NOT NULL DEFAULT 'manager',"
                 " created_at BIGINT NOT NULL DEFAULT 0)",
                 {});
    db_->execute("CREATE TABLE IF NOT EXISTS " + db_->table("admin_log") + " ("
                 " log_id INTEGER PRIMARY KEY AUTOINCREMENT,"
                 " admin_id BIGINT NOT NULL,"
                 " action VARCHAR(64) NOT NULL,"
                 " detail VARCHAR(512) NOT NULL DEFAULT ''," 
                 " ip_address VARCHAR(45) NOT NULL DEFAULT '',"
                 " created_at BIGINT NOT NULL DEFAULT 0)",
                 {});
    db_->execute("CREATE TABLE IF NOT EXISTS " + db_->table("admin_sessions") + " ("
                 " token_hash CHAR(64) PRIMARY KEY,"
                 " admin_id BIGINT NOT NULL,"
                 " created_at BIGINT NOT NULL DEFAULT 0)",
                 {});
}

std::optional<domain::AdminUser> SqlAdminRepository::findByUsername(const std::string &username)
{
    std::string sql =
        "SELECT admin_id AS admin_id, user_name AS user_name, role AS role FROM " +
        db_->table("admin_user") + " WHERE user_name = ?";
    std::vector<Row> rows = db_->query(sql, {username});
    if (rows.empty())
        return std::nullopt;

    domain::AdminUser admin;
    admin.admin_id = rows.front().getInt("admin_id");
    admin.username = rows.front().get("user_name");
    admin.role = rows.front().get("role");
    return admin;
}

std::optional<domain::AdminUser> SqlAdminRepository::findById(int64_t admin_id)
{
    std::string sql =
        "SELECT admin_id AS admin_id, user_name AS user_name, role AS role FROM " +
        db_->table("admin_user") + " WHERE admin_id = ?";
    std::vector<Row> rows = db_->query(sql, {std::to_string(admin_id)});
    if (rows.empty())
        return std::nullopt;

    domain::AdminUser admin;
    admin.admin_id = rows.front().getInt("admin_id");
    admin.username = rows.front().get("user_name");
    admin.role = rows.front().get("role");
    return admin;
}

std::optional<std::string> SqlAdminRepository::passwordHashOf(int64_t admin_id)
{
    std::string sql =
        "SELECT password_hash AS password_hash FROM " + db_->table("admin_user") +
        " WHERE admin_id = ?";
    std::vector<Row> rows = db_->query(sql, {std::to_string(admin_id)});
    if (rows.empty())
        return std::nullopt;
    return rows.front().get("password_hash");
}

void SqlAdminRepository::ensureAdmin(const std::string &username,
                                     const std::string &password_hash, const std::string &role)
{
    if (findByUsername(username))
        return;
    std::string sql =
        "INSERT INTO " + db_->table("admin_user") +
        " (user_name, password_hash, role) VALUES (?, ?, ?)";
    db_->execute(sql, {username, password_hash, role});
}

std::vector<domain::AdminUser> SqlAdminRepository::listAdmins()
{
    std::string sql =
        "SELECT admin_id AS admin_id, user_name AS user_name, role AS role FROM " +
        db_->table("admin_user") + " ORDER BY admin_id";

    std::vector<domain::AdminUser> items;
    for (const Row &row : db_->query(sql, {}))
    {
        domain::AdminUser admin;
        admin.admin_id = row.getInt("admin_id");
        admin.username = row.get("user_name");
        admin.role = row.get("role");
        items.push_back(std::move(admin));
    }
    return items;
}

int64_t SqlAdminRepository::createAdmin(const std::string &username,
                                        const std::string &password_hash,
                                        const std::string &role)
{
    std::string sql =
        "INSERT INTO " + db_->table("admin_user") +
        " (user_name, password_hash, role) VALUES (?, ?, ?)";
    db_->execute(sql, {username, password_hash, role});
    return db_->lastInsertId();
}

bool SqlAdminRepository::deleteAdmin(int64_t admin_id)
{
    bool ok = false;
    db_->transaction([&] {
        // revoke all sessions first, then drop the row
        db_->execute("DELETE FROM " + db_->table("admin_sessions") + " WHERE admin_id = ?",
                     {std::to_string(admin_id)});
        ok = db_->execute("DELETE FROM " + db_->table("admin_user") +
                              " WHERE admin_id = ?",
                          {std::to_string(admin_id)}) > 0;
    });
    return ok;
}

void SqlAdminRepository::createSession(const std::string &token_hash, int64_t admin_id)
{
    std::string sql = "INSERT INTO " + db_->table("admin_sessions") +
                      " (token_hash, admin_id) VALUES (?, ?)";
    db_->execute(sql, {token_hash, std::to_string(admin_id)});
}

std::optional<int64_t> SqlAdminRepository::adminIdOfTokenHash(const std::string &token_hash)
{
    std::string sql = "SELECT admin_id AS admin_id FROM " + db_->table("admin_sessions") +
                      " WHERE token_hash = ?";
    std::vector<Row> rows = db_->query(sql, {token_hash});
    if (rows.empty())
        return std::nullopt;
    return rows.front().getInt("admin_id");
}

void SqlAdminRepository::deleteSession(const std::string &token_hash)
{
    std::string sql = "DELETE FROM " + db_->table("admin_sessions") + " WHERE token_hash = ?";
    db_->execute(sql, {token_hash});
}

void SqlAdminRepository::writeLog(int64_t admin_id, const std::string &action,
                                  const std::string &detail, const std::string &ip_address)
{
    std::string sql = "INSERT INTO " + db_->table("admin_log") +
                      " (admin_id, action, detail, ip_address, created_at)"
                      " VALUES (?, ?, ?, ?, ?)";
    std::string now = std::to_string(shared::nowUnix());
    db_->execute(sql, {std::to_string(admin_id), action, detail, ip_address, now});
}

std::vector<domain::AdminLogRow> SqlAdminRepository::listLogs(int64_t offset, int64_t limit)
{
    std::string sql =
        "SELECT lg.log_id AS log_id, lg.admin_id AS admin_id, au.user_name AS admin_name,"
        " lg.action AS action, lg.detail AS detail, lg.ip_address AS ip_address," +
        db_->toUnix("lg.created_at") + " AS created_at FROM " + db_->table("admin_log") +
        " lg LEFT JOIN " + db_->table("admin_user") + " au ON au.admin_id = lg.admin_id"
        " ORDER BY lg.log_id DESC LIMIT " + std::to_string(limit) + " OFFSET " +
        std::to_string(offset);

    std::vector<domain::AdminLogRow> items;
    for (const Row &row : db_->query(sql, {}))
    {
        domain::AdminLogRow item;
        item.log_id = row.getInt("log_id");
        item.admin_id = row.getInt("admin_id");
        item.admin_name = row.get("admin_name");
        item.action = row.get("action");
        item.detail = row.get("detail");
        item.ip_address = row.get("ip_address");
        item.created_at = row.get("created_at");
        items.push_back(std::move(item));
    }
    return items;
}

} // namespace ecshop::infra
