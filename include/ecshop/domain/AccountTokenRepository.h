#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>

namespace ecshop::domain {

// One-shot tokens (email verification, password reset) and the email outbox.
// Only SHA-256 hashes of tokens are persisted; plaintext goes into the outbox
// payload for the mail worker.
class AccountTokenRepository
{
public:
    virtual ~AccountTokenRepository() = default;

    // mail worker queue; payload_json is stored verbatim
    virtual void queueEmail(int64_t user_id, const std::string &recipient,
                            const std::string &template_name,
                            const std::string &payload_json) = 0;

    // replaces any previous token of the user
    virtual void replaceEmailVerification(int64_t user_id, const std::string &token_hash,
                                          int64_t expires_at) = 0;
    // marks the user verified and consumes the token; nullopt when the token is
    // missing, expired or already consumed
    virtual std::optional<int64_t> consumeEmailVerification(const std::string &token_hash,
                                                            int64_t now) = 0;

    virtual void replacePasswordReset(int64_t user_id, const std::string &token_hash,
                                      int64_t expires_at) = 0;
    // sets the new password hash and consumes the token in one transaction
    virtual std::optional<int64_t> consumePasswordReset(const std::string &token_hash,
                                                        const std::string &new_password_hash,
                                                        int64_t now) = 0;
};

} // namespace ecshop::domain
