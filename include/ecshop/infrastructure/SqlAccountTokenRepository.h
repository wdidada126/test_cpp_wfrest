#pragma once

#include <memory>
#include <optional>
#include <string>

#include "ecshop/domain/AccountTokenRepository.h"
#include "ecshop/infrastructure/db/Db.h"

namespace ecshop::infra {

class SqlAccountTokenRepository : public domain::AccountTokenRepository
{
public:
    explicit SqlAccountTokenRepository(std::shared_ptr<Db> db) : db_(std::move(db)) {}

    void queueEmail(int64_t user_id, const std::string &recipient,
                    const std::string &template_name,
                    const std::string &payload_json) override;
    void replaceEmailVerification(int64_t user_id, const std::string &token_hash,
                                  int64_t expires_at) override;
    std::optional<int64_t> consumeEmailVerification(const std::string &token_hash,
                                                    int64_t now) override;
    void replacePasswordReset(int64_t user_id, const std::string &token_hash,
                              int64_t expires_at) override;
    std::optional<int64_t> consumePasswordReset(const std::string &token_hash,
                                                const std::string &new_password_hash,
                                                int64_t now) override;

private:
    std::shared_ptr<Db> db_;
};

} // namespace ecshop::infra
