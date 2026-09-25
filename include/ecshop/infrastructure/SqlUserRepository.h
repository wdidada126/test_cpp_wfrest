#pragma once

#include "ecshop/domain/UserRepository.h"
#include "ecshop/infrastructure/db/Db.h"

namespace ecshop::infra {

class SqlUserRepository : public domain::UserRepository
{
public:
    explicit SqlUserRepository(std::shared_ptr<Db> db) : db_(std::move(db)) {}

    std::optional<domain::User> findById(int64_t user_id) override;
    std::optional<domain::User> findByUsername(const std::string &username) override;
    std::optional<domain::User> findByEmail(const std::string &email) override;
    bool updateEmail(int64_t user_id, const std::string &email) override;
    std::optional<std::string> passwordHashOf(int64_t user_id) override;
    bool updatePasswordIfHashMatches(int64_t user_id, const std::string &expected_hash,
                                     const std::string &new_hash) override;
    int64_t createUser(const std::string &username, const std::string &email,
                       const std::string &password_hash) override;
    void createSession(const std::string &token_hash, int64_t user_id) override;
    std::optional<int64_t> userIdOfTokenHash(const std::string &token_hash) override;
    void deleteSession(const std::string &token_hash) override;
    void deleteSessionsOfUser(int64_t user_id) override;

private:
    std::shared_ptr<Db> db_;
};

} // namespace ecshop::infra
