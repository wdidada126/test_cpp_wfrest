#pragma once

#include "ecshop/domain/AdminRepository.h"
#include "ecshop/infrastructure/db/Db.h"

#include <memory>

namespace ecshop::infra {

class SqlAdminRepository : public domain::AdminRepository
{
public:
    explicit SqlAdminRepository(std::shared_ptr<Db> db) : db_(std::move(db)) {}

    void ensureSchema() override;
    std::optional<domain::AdminUser> findByUsername(const std::string &username) override;
    std::optional<domain::AdminUser> findById(int64_t admin_id) override;
    std::optional<std::string> passwordHashOf(int64_t admin_id) override;
    void ensureAdmin(const std::string &username, const std::string &password_hash,
                     const std::string &role) override;
    std::vector<domain::AdminUser> listAdmins() override;
    int64_t createAdmin(const std::string &username, const std::string &password_hash,
                        const std::string &role) override;
    bool deleteAdmin(int64_t admin_id) override;
    void createSession(const std::string &token_hash, int64_t admin_id) override;
    std::optional<int64_t> adminIdOfTokenHash(const std::string &token_hash) override;
    void deleteSession(const std::string &token_hash) override;
    void writeLog(int64_t admin_id, const std::string &action, const std::string &detail,
                  const std::string &ip_address) override;
    std::vector<domain::AdminLogRow> listLogs(int64_t offset, int64_t limit) override;

private:
    std::shared_ptr<Db> db_;
};

} // namespace ecshop::infra
