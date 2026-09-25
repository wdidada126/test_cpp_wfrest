#include "ecshop/infrastructure/SqlAccountRepository.h"
#include "ecshop/shared/Money.h"

namespace ecshop::infra {

using domain::AccountRequest;
using domain::Balance;

static AccountRequest rowToRequest(const Row &row)
{
    AccountRequest request;
    request.id = row.getInt("rec_id");
    request.kind = row.get("process_type");
    request.amount = shared::Money::format(row.getInt("amount_cents"));
    request.status = row.get("status");
    request.payment_id = row.getInt("payment_id");
    request.note = row.get("user_note");
    request.created_at = row.get("created_at");
    request.paid_at = row.get("paid_at");
    return request;
}

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

Balance SqlAccountRepository::balanceOf(int64_t user_id)
{
    std::string sql = "SELECT available_cents AS available_cents, frozen_cents AS frozen_cents"
                      " FROM " + db_->table("account_balance") + " WHERE user_id = ?";
    std::vector<Row> rows = db_->query(sql, {std::to_string(user_id)});

    Balance balance;
    if (rows.empty())
    {
        balance.available = "0.00";
        balance.frozen = "0.00";
        return balance;
    }
    balance.available = shared::Money::format(rows.front().getInt("available_cents"));
    balance.frozen = shared::Money::format(rows.front().getInt("frozen_cents"));
    return balance;
}

domain::AccountRequestPage SqlAccountRepository::listRequests(int64_t user_id, int64_t offset,
                                                              int64_t limit)
{
    domain::AccountRequestPage page;
    page.balance = balanceOf(user_id);

    const std::string account_t = db_->table("user_account");
    std::vector<Row> counts = db_->query(
        "SELECT COUNT(*) AS total FROM " + account_t + " WHERE user_id = ?",
        {std::to_string(user_id)});
    if (!counts.empty())
        page.total = counts.front().getInt("total");

    // limit/offset are validated integers formatted by us; user data stays bound.
    std::string sql =
        "SELECT rec_id AS rec_id, process_type AS process_type, amount_cents AS amount_cents,"
        " status AS status, payment_id AS payment_id, user_note AS user_note," +
        db_->toUnix("created_at") + " AS created_at, " +
        db_->toUnix("paid_at") + " AS paid_at FROM " + account_t +
        " WHERE user_id = ? ORDER BY rec_id DESC LIMIT " + std::to_string(limit) +
        " OFFSET " + std::to_string(offset);

    for (const Row &row : db_->query(sql, {std::to_string(user_id)}))
        page.items.push_back(rowToRequest(row));
    return page;
}

bool SqlAccountRepository::cancelRequest(int64_t user_id, int64_t request_id)
{
    std::string account_t = db_->table("user_account");
    std::string balance_t = db_->table("account_balance");
    std::string log_t = db_->table("account_log");

    bool ok = false;
    db_->transaction([&] {
        std::vector<Row> rows = db_->query(
            "SELECT amount_cents AS amount_cents, process_type AS process_type, status AS status"
            " FROM " + account_t + " WHERE rec_id = ? AND user_id = ?",
            {std::to_string(request_id), std::to_string(user_id)});
        if (rows.empty())
            return;

        const Row &row = rows.front();
        std::string kind = row.get("process_type");
        std::string status = row.get("status");
        int64_t amount_cents = row.getInt("amount_cents");

        bool cancellable =
            (kind == "deposit" && status == "pending_payment") ||
            (kind == "withdrawal" && status == "pending_review");
        if (!cancellable)
            return;

        if (kind == "withdrawal")
        {
            int64_t changed = db_->execute(
                "UPDATE " + balance_t +
                    " SET available_cents = available_cents + ?,"
                    " frozen_cents = frozen_cents - ?"
                    " WHERE user_id = ? AND frozen_cents >= ?",
                {std::to_string(amount_cents), std::to_string(amount_cents),
                 std::to_string(user_id), std::to_string(amount_cents)});
            if (changed == 0)
                return;

            db_->execute("INSERT INTO " + log_t +
                             " (user_id, available_delta_cents, frozen_delta_cents, reason,"
                             " reference_type, reference_id)"
                             " VALUES (?, ?, ?, 'withdrawal cancelled', 'user_account', ?)",
                         {std::to_string(user_id), std::to_string(amount_cents),
                          std::to_string(-amount_cents), std::to_string(request_id)});
        }

        db_->execute("DELETE FROM " + account_t + " WHERE rec_id = ? AND user_id = ?",
                     {std::to_string(request_id), std::to_string(user_id)});
        ok = true;
    });
    return ok;
}

} // namespace ecshop::infra
