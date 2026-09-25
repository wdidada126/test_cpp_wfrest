#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace ecshop::domain {

struct AdminUser
{
    int64_t admin_id = 0;
    std::string username;
    std::string role; // "super" | "manager"
};

struct AdminLogRow
{
    int64_t log_id = 0;
    int64_t admin_id = 0;
    std::string admin_name;
    std::string action;
    std::string detail;
    std::string ip_address;
    std::string created_at; // unix seconds as text
};

class AdminRepository
{
public:
    virtual ~AdminRepository() = default;

    // optional tables are created with IF NOT EXISTS on first use
    virtual void ensureSchema() = 0;

    virtual std::optional<AdminUser> findByUsername(const std::string &username) = 0;
    virtual std::optional<AdminUser> findById(int64_t admin_id) = 0;
    virtual std::optional<std::string> passwordHashOf(int64_t admin_id) = 0;
    virtual void ensureAdmin(const std::string &username, const std::string &password_hash,
                             const std::string &role) = 0;

    virtual void createSession(const std::string &token_hash, int64_t admin_id) = 0;
    virtual std::optional<int64_t> adminIdOfTokenHash(const std::string &token_hash) = 0;
    virtual void deleteSession(const std::string &token_hash) = 0;

    virtual void writeLog(int64_t admin_id, const std::string &action,
                          const std::string &detail, const std::string &ip_address) = 0;

    virtual std::vector<AdminLogRow> listLogs(int64_t offset, int64_t limit) = 0;
};

} // namespace ecshop::domain
