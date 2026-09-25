#include "ecshop/infrastructure/SqlAccountTokenRepository.h"

namespace ecshop::infra {

void SqlAccountTokenRepository::queueEmail(int64_t user_id, const std::string &recipient,
                                           const std::string &template_name,
                                           const std::string &payload_json)
{
    std::string sql = "INSERT INTO " + db_->table("email_outbox") +
                      " (user_id, recipient, template_name, payload) VALUES (?, ?, ?, ?)";
    db_->execute(sql, {std::to_string(user_id), recipient, template_name, payload_json});
}

void SqlAccountTokenRepository::replaceEmailVerification(int64_t user_id,
                                                          const std::string &token_hash,
                                                          int64_t expires_at)
{
    std::string tokens_t = db_->table("email_verification_tokens");
    db_->execute("DELETE FROM " + tokens_t + " WHERE user_id = ?", {std::to_string(user_id)});
    db_->execute("INSERT INTO " + tokens_t +
                     " (user_id, token_hash, expires_at) VALUES (?, ?, ?)",
                 {std::to_string(user_id), token_hash,
                  db_->datetimeFromUnix(expires_at)});
}

std::optional<int64_t> SqlAccountTokenRepository::consumeEmailVerification(
    const std::string &token_hash, int64_t now)
{
    std::string tokens_t = db_->table("email_verification_tokens");
    std::string verified_t = db_->table("email_verified_users");

    std::optional<int64_t> user_id;
    db_->transaction([&] {
        std::vector<Row> rows = db_->query(
            "SELECT user_id AS user_id FROM " + tokens_t +
                " WHERE token_hash = ? AND consumed_at IS NULL AND expires_at > " +
                db_->nowExpr(),
            {token_hash});
        if (rows.empty())
            return;

        user_id = rows.front().getInt("user_id");

        db_->execute("DELETE FROM " + verified_t + " WHERE user_id = ?",
                     {std::to_string(*user_id)});
        db_->execute("INSERT INTO " + verified_t + " (user_id, verified_at) VALUES (?, ?)",
                     {std::to_string(*user_id), db_->datetimeFromUnix(now)});
        db_->execute("UPDATE " + tokens_t +
                         " SET consumed_at = ? WHERE token_hash = ? AND consumed_at IS NULL",
                     {db_->datetimeFromUnix(now), token_hash});
    });
    return user_id;
}

void SqlAccountTokenRepository::replacePasswordReset(int64_t user_id,
                                                      const std::string &token_hash,
                                                      int64_t expires_at)
{
    std::string tokens_t = db_->table("password_reset_tokens");
    db_->execute("DELETE FROM " + tokens_t + " WHERE user_id = ?", {std::to_string(user_id)});
    db_->execute("INSERT INTO " + tokens_t + " (user_id, token_hash, expires_at) VALUES (?, ?, ?)",
                 {std::to_string(user_id), token_hash, db_->datetimeFromUnix(expires_at)});
}

std::optional<int64_t> SqlAccountTokenRepository::consumePasswordReset(
    const std::string &token_hash, const std::string &new_password_hash, int64_t now)
{
    std::string tokens_t = db_->table("password_reset_tokens");
    std::string users_t = db_->table("users");
    std::string sessions_t = db_->table("sessions");

    std::optional<int64_t> user_id;
    db_->transaction([&] {
        std::vector<Row> rows = db_->query(
            "SELECT user_id AS user_id FROM " + tokens_t +
                " WHERE token_hash = ? AND consumed_at IS NULL AND expires_at > " +
                db_->nowExpr(),
            {token_hash});
        if (rows.empty())
            return;

        user_id = rows.front().getInt("user_id");

        db_->execute("UPDATE " + users_t + " SET password_hash = ? WHERE user_id = ?",
                     {new_password_hash, std::to_string(*user_id)});
        db_->execute("DELETE FROM " + sessions_t + " WHERE user_id = ?",
                     {std::to_string(*user_id)});
        db_->execute("UPDATE " + tokens_t +
                         " SET consumed_at = ? WHERE token_hash = ? AND consumed_at IS NULL",
                     {db_->datetimeFromUnix(now), token_hash});
    });
    return user_id;
}

} // namespace ecshop::infra
