#pragma once

#include "ecshop/domain/NewsletterRepository.h"
#include "ecshop/infrastructure/db/Db.h"

#include <memory>

namespace ecshop::infra {

class SqlNewsletterRepository : public domain::NewsletterRepository
{
public:
    explicit SqlNewsletterRepository(std::shared_ptr<Db> db) : db_(std::move(db)) {}

    bool requestSubscribe(const std::string &email, const std::string &token_hash,
                          const std::string &plaintext_token, int64_t expires_at) override;
    bool requestUnsubscribe(const std::string &email, const std::string &token_hash,
                            const std::string &plaintext_token, int64_t expires_at) override;
    bool confirmSubscribe(const std::string &token_hash, int64_t now) override;
    bool confirmUnsubscribe(const std::string &token_hash, int64_t now) override;

private:
    // outbox row with NULL user_id (public newsletter flow)
    void queueMail(const std::string &recipient, const std::string &template_name,
                   const std::string &payload_json);

    std::shared_ptr<Db> db_;
};

} // namespace ecshop::infra
