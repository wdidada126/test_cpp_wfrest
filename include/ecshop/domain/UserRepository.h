#pragma once

#include "ecshop/domain/User.h"

#include <memory>
#include <optional>
#include <string>

namespace ecshop::domain {

// Users and sessions. Password hashes and token hashes never leave the
// infrastructure layer in plaintext form.
class UserRepository
{
public:
    virtual ~UserRepository() = default;

    virtual std::optional<User> findById(int64_t user_id) = 0;
    virtual std::optional<User> findByUsername(const std::string &username) = 0;
    virtual std::optional<User> findByEmail(const std::string &email) = 0;

    // profile updates; returns false when email is taken by another user
    virtual bool updateEmail(int64_t user_id, const std::string &email) = 0;

    // password hash of the row, for verifyPassword()
    virtual std::optional<std::string> passwordHashOf(int64_t user_id) = 0;

    // atomic compare-and-swap on the stored hash; false when it changed meanwhile
    virtual bool updatePasswordIfHashMatches(int64_t user_id, const std::string &expected_hash,
                                             const std::string &new_hash) = 0;

    // returns new user id; throws DbError when unique constraints reject it
    virtual int64_t createUser(const std::string &username, const std::string &email,
                               const std::string &password_hash) = 0;

    // sessions: only the SHA-256 hash of the token is stored
    virtual void createSession(const std::string &token_hash, int64_t user_id) = 0;
    virtual std::optional<int64_t> userIdOfTokenHash(const std::string &token_hash) = 0;
    virtual void deleteSession(const std::string &token_hash) = 0;
    virtual void deleteSessionsOfUser(int64_t user_id) = 0;
};

} // namespace ecshop::domain
