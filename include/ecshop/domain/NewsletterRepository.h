#pragma once

#include <cstdint>
#include <string>

namespace ecshop::domain {

// Newsletter subscription flow built on email_list + transactional outbox.
// Only SHA-256 token hashes are stored; plaintext goes to the outbox payload.
class NewsletterRepository
{
public:
    virtual ~NewsletterRepository() = default;

    // returns true when a confirmation mail was queued; false when the address
    // is already subscribed (idempotent accept, no mail)
    virtual bool requestSubscribe(const std::string &email, const std::string &token_hash,
                                  const std::string &plaintext_token, int64_t expires_at) = 0;

    // returns true when a confirmation mail was queued; false when already
    // unsubscribed (idempotent accept, no mail)
    virtual bool requestUnsubscribe(const std::string &email, const std::string &token_hash,
                                    const std::string &plaintext_token, int64_t expires_at) = 0;

    virtual bool confirmSubscribe(const std::string &token_hash, int64_t now) = 0;
    virtual bool confirmUnsubscribe(const std::string &token_hash, int64_t now) = 0;
};

} // namespace ecshop::domain
