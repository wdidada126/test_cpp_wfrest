#include "ecshop/infrastructure/SqlAccountRepository.h"

namespace ecshop::infra {

bool SqlAccountRepository::paymentEnabled(int64_t payment_id)
{
    std::string sql = "SELECT pay_id AS pay_id FROM " + db_->table("payment") +
                      " WHERE pay_id = ? AND enabled = 1";
    return !db_->query(sql, {std::to_string(payment_id)}).empty();
}

void SqlAccountRepository::ensureBalance(int64_t user_id)
{
    // portable "insert if missing" (no INSERT OR IGNORE / INSERT IGNORE split)
    std::string sql = "INSERT INTO " + db_->table("account_balance") +
                      " (user_id, available_cents, frozen_cents)"
                      " SELECT ?, 0, 0 WHERE NOT EXISTS (SELECT 1 FROM " +
                      db_->table("account_balance") + " WHERE user_id = ?)";
    db_->execute(sql, {std::to_string(user_id), std::to_string(user_id)});
}

int64_t SqlAccountRepository::createDepositRequest(int64_t user_id, int64_t amount_cents,
                                                   int64_t payment_id, const std::string &note)
{
    std::string sql = "INSERT INTO " + db_->table("user_account") +
                      " (user_id, amount_cents, process_type, payment_id, user_note, status)"
                      " VALUES (?, ?, 'deposit', ?, ?, 'pending_payment')";
    db_->execute(sql, {std::to_string(user_id), std::to_string(amount_cents),
                       std::to_string(payment_id), note});
    return db_->lastInsertId();
}

bool SqlAccountRepository::createWithdrawalRequest(int64_t user_id, int64_t amount_cents,
                                                   const std::string &note, int64_t &request_id)
{
    request_id = 0;
    std::string balance_t = db_->table("account_balance");
    std::string account_t = db_->table("user_account");
    std::string log_t = db_->table("account_log");

    bool ok = false;
    db_->transaction([&] {
        ensureBalance(user_id);

        ok = db_->execute(
                 "UPDATE " + balance_t +
                     " SET available_cents = available_cents - ?,"
                     " frozen_cents = frozen_cents + ?"
                     " WHERE user_id = ? AND available_cents >= ?",
                 {std::to_string(amount_cents), std::to_string(amount_cents),
                  std::to_string(user_id), std::to_string(amount_cents)}) > 0;
        if (!ok)
            return;

        db_->execute("INSERT INTO " + account_t +
                         " (user_id, amount_cents, process_type, user_note, status)"
                         " VALUES (?, ?, 'withdrawal', ?, 'pending_review')",
                     {std::to_string(user_id), std::to_string(amount_cents), note});
        request_id = db_->lastInsertId();

        db_->execute("INSERT INTO " + log_t +
                         " (user_id, available_delta_cents, frozen_delta_cents, reason,"
                         " reference_type, reference_id) VALUES (?, ?, ?, ?, 'user_account', ?)",
                     {std::to_string(user_id), std::to_string(-amount_cents),
                      std::to_string(amount_cents),
                      "withdrawal requested", std::to_string(request_id)});
    });
    return ok;
}

} // namespace ecshop::infra
