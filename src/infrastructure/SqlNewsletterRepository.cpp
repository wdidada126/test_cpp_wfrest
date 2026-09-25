#include "ecshop/infrastructure/SqlNewsletterRepository.h"

namespace ecshop::infra {

void SqlNewsletterRepository::queueMail(const std::string &recipient,
                                        const std::string &template_name,
                                        const std::string &payload_json)
{
    // public flow: no user_id (NULL) is bound to the outbox row
    std::string sql = "INSERT INTO " + db_->table("email_outbox") +
                      " (recipient, template_name, payload) VALUES (?, ?, ?)";
    db_->execute(sql, {recipient, template_name, payload_json});
}

bool SqlNewsletterRepository::requestSubscribe(const std::string &email,
                                               const std::string &token_hash,
                                               const std::string &plaintext_token,
                                               int64_t expires_at)
{
    const std::string list_t = db_->table("email_list");

    bool queued = true;
    db_->transaction([&] {
        std::vector<Row> rows = db_->query(
            "SELECT status AS status, pending_action AS pending_action FROM " + list_t +
                " WHERE email = ?",
            {email});

        if (!rows.empty() && rows.front().getInt("status") == 1 &&
            rows.front().get("pending_action") == "")
        {
            queued = false; // already subscribed; idempotent accept without mail
            return;
        }

        // free the unique email slot, then re-issue the one-time token
        db_->execute("DELETE FROM " + list_t + " WHERE email = ?", {email});
        db_->execute("INSERT INTO " + list_t +
                         " (email, status, token_hash, pending_action, token_expires_at)"
                         " VALUES (?, ?, ?, ?, ?)",
                     {email, "0", token_hash, "subscribe", db_->datetimeFromUnix(expires_at)});
        queueMail(email, "newsletter_subscribe", "{\"token\":\"" + plaintext_token + "\"}");
    });
    return queued;
}

bool SqlNewsletterRepository::requestUnsubscribe(const std::string &email,
                                                 const std::string &token_hash,
                                                 const std::string &plaintext_token,
                                                 int64_t expires_at)
{
    const std::string list_t = db_->table("email_list");

    bool queued = true;
    db_->transaction([&] {
        std::vector<Row> rows = db_->query(
            "SELECT status AS status, pending_action AS pending_action FROM " + list_t +
                " WHERE email = ?",
            {email});

        if (rows.empty() || rows.front().getInt("status") == 0)
        {
            bool pending_unsub = !rows.empty() && rows.front().get("pending_action") == "unsubscribe";
            if (!pending_unsub)
            {
                queued = false; // not subscribed; idempotent accept without mail
                return;
            }
        }

        db_->execute("DELETE FROM " + list_t + " WHERE email = ?", {email});
        db_->execute("INSERT INTO " + list_t +
                         " (email, status, token_hash, pending_action, token_expires_at)"
                         " VALUES (?, ?, ?, ?, ?)",
                     {email, "1", token_hash, "unsubscribe", db_->datetimeFromUnix(expires_at)});
        queueMail(email, "newsletter_unsubscribe", "{\"token\":\"" + plaintext_token + "\"}");
    });
    return queued;
}

bool SqlNewsletterRepository::confirmSubscribe(const std::string &token_hash, int64_t now)
{
    const std::string list_t = db_->table("email_list");
    int64_t changed = db_->execute(
        "UPDATE " + list_t + " SET status = 1, pending_action = ''"
                             " WHERE token_hash = ? AND pending_action = 'subscribe'"
                             " AND token_expires_at > " + db_->nowExpr(),
        {token_hash});
    return changed > 0;
}

bool SqlNewsletterRepository::confirmUnsubscribe(const std::string &token_hash, int64_t now)
{
    const std::string list_t = db_->table("email_list");
    int64_t changed = db_->execute(
        "DELETE FROM " + list_t +
            " WHERE token_hash = ? AND pending_action = 'unsubscribe'"
            " AND token_expires_at > " + db_->nowExpr(),
        {token_hash});
    return changed > 0;
}

} // namespace ecshop::infra
