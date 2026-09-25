#include "ecshop/infrastructure/SqlAdminExtraRepository.h"
#include "ecshop/shared/Money.h"
#include "ecshop/shared/TimeUtil.h"

namespace ecshop::infra {

using namespace domain;

SqlAdminExtraRepository::AdminUserRowPage SqlAdminExtraRepository::listUsers(int64_t offset,
                                                                            int64_t limit)
{
    AdminUserRowPage page;

    std::vector<Row> counts = db_->query(
        "SELECT COUNT(*) AS total FROM " + db_->table("users"), {});
    page.total = counts.empty() ? 0 : counts.front().getInt("total");

    std::string sql =
        "SELECT u.user_id AS user_id, u.user_name AS user_name, u.email AS email," +
        db_->toUnix("u." + std::string("created_at")) + " AS created_at" +
        " FROM " + db_->table("users") + " u ORDER BY user_id DESC LIMIT " +
        std::to_string(limit) + " OFFSET " + std::to_string(offset);

    for (const Row &row : db_->query(sql, {}))
    {
        AdminUserRow item;
        item.user_id = row.getInt("user_id");
        item.username = row.get("user_name");
        item.email = row.get("email");
        item.created_at = row.get("created_at");
        page.items.push_back(std::move(item));
    }
    return page;
}

bool SqlAdminExtraRepository::updateUserEmail(int64_t user_id, const std::string &email)
{
    return db_->execute("UPDATE " + db_->table("users") + " SET email = ? WHERE user_id = ?",
                        {email, std::to_string(user_id)}) > 0;
}

int64_t SqlAdminExtraRepository::createPromotion(const std::string &name,
                                                 const std::string &description,
                                                 int64_t act_type, int64_t goods_id,
                                                 int64_t start_time, int64_t end_time,
                                                 const std::string &ext_info)
{
    std::string sql = "INSERT INTO " + db_->table("goods_activity") +
                      " (act_name, act_desc, act_type, goods_id, goods_name,"
                      " start_time, end_time, is_finished, ext_info) VALUES (?, ?, ?, ?, ?, ?, ?, 0, ?)";

    std::vector<Row> goods = db_->query(
        "SELECT goods_name AS goods_name FROM " + db_->table("goods") + " WHERE goods_id = ?",
        {std::to_string(goods_id)});
    std::string goods_name = goods.empty() ? std::string() : goods.front().get("goods_name");

    std::string start = std::to_string(start_time);
    std::string end = std::to_string(end_time);
    db_->execute(sql, {name, description, std::to_string(act_type), std::to_string(goods_id),
                       goods_name, start, end, ext_info});
    return db_->lastInsertId();
}

bool SqlAdminExtraRepository::deletePromotion(int64_t act_id, int64_t act_type)
{
    return db_->execute("DELETE FROM " + db_->table("goods_activity") +
                            " WHERE act_id = ? AND act_type = ?",
                        {std::to_string(act_id), std::to_string(act_type)}) > 0;
}

std::optional<SqlAdminExtraRepository::SettleRequest>
SqlAdminExtraRepository::findAccountRequest(int64_t rec_id)
{
    std::string sql =
        "SELECT rec_id AS rec_id, process_type AS process_type, amount_cents AS amount_cents,"
        " status AS status FROM " + db_->table("user_account") + " WHERE rec_id = ?";

    std::vector<Row> rows = db_->query(sql, {std::to_string(rec_id)});
    if (rows.empty())
        return std::nullopt;
    const Row &row = rows.front();
    SettleRequest request;
    request.rec_id = row.getInt("rec_id");
    request.process_type = row.get("process_type");
    request.amount_cents = row.getInt("amount_cents");
    request.status = row.get("status");
    return request;
}

bool SqlAdminExtraRepository::settleDeposit(int64_t rec_id, int64_t amount_cents)
{
    std::string account_t = db_->table("user_account");
    std::string balance_t = db_->table("account_balance");
    std::string log_t = db_->table("account_log");

    bool ok = false;
    db_->transaction([&] {
        std::vector<Row> rows = db_->query(
            "SELECT user_id AS user_id, amount_cents AS amount_cents, status AS status"
            " FROM " + account_t + " WHERE rec_id = ? AND process_type = 'deposit'",
            {std::to_string(rec_id)});
        if (rows.empty())
            return;

        int64_t user_id = rows.front().getInt("user_id");
        int64_t stored_amount = rows.front().getInt("amount_cents");

        // fund only if the request was never settled before
        int64_t changed = db_->execute(
            "UPDATE " + account_t + " SET status = 'paid', paid_at = " + db_->nowExpr() +
                " WHERE rec_id = ? AND status = 'pending_payment'",
            {std::to_string(rec_id)});
        if (changed == 0)
            return;

        db_->execute("INSERT INTO " + balance_t +
                         " (user_id, available_cents, frozen_cents) SELECT ?, ?, 0"
                         " WHERE NOT EXISTS (SELECT 1 FROM " + balance_t +
                         " WHERE user_id = ?)",
                     {std::to_string(user_id), std::to_string(stored_amount),
                      std::to_string(user_id)});
        db_->execute("UPDATE " + balance_t +
                         " SET available_cents = available_cents + ? WHERE user_id = ?",
                     {std::to_string(stored_amount), std::to_string(user_id)});
        db_->execute("INSERT INTO " + log_t +
                         " (user_id, available_delta_cents, frozen_delta_cents, reason,"
                         " reference_type, reference_id)"
                         " VALUES (?, ?, 0, 'deposit settled', 'user_account', ?)",
                     {std::to_string(user_id), std::to_string(stored_amount),
                      std::to_string(rec_id)});
        (void)amount_cents;
        ok = true;
    });
    return ok;
}

bool SqlAdminExtraRepository::settleWithdrawal(int64_t rec_id, int64_t amount_cents)
{
    std::string account_t = db_->table("user_account");
    std::string balance_t = db_->table("account_balance");
    std::string log_t = db_->table("account_log");

    bool ok = false;
    db_->transaction([&] {
        std::vector<Row> rows = db_->query(
            "SELECT user_id AS user_id, amount_cents AS amount_cents FROM " + account_t +
                " WHERE rec_id = ? AND process_type = 'withdrawal'",
            {std::to_string(rec_id)});
        if (rows.empty())
            return;

        int64_t user_id = rows.front().getInt("user_id");
        int64_t stored_amount = rows.front().getInt("amount_cents");

        // disburse: frozen -> withdrawn; conditional on frozen balance
        int64_t changed = db_->execute(
            "UPDATE " + account_t + " SET status = 'disbursed', paid_at = " + db_->nowExpr() +
                " WHERE rec_id = ? AND status = 'pending_review'",
            {std::to_string(rec_id)});
        if (changed == 0)
            return;

        int64_t balance_changed = db_->execute(
            "UPDATE " + balance_t +
                " SET frozen_cents = frozen_cents - ? WHERE user_id = ? AND frozen_cents >= ?",
            {std::to_string(stored_amount), std::to_string(user_id),
             std::to_string(stored_amount)});
        if (balance_changed == 0)
        {
            db_->execute("UPDATE " + account_t + " SET status = 'pending_review'" +
                             ", paid_at = NULL WHERE rec_id = ?",
                         {std::to_string(rec_id)});
            return;
        }
        db_->execute("INSERT INTO " + log_t +
                         " (user_id, available_delta_cents, frozen_delta_cents, reason,"
                         " reference_type, reference_id)"
                         " VALUES (?, 0, ?, 'withdrawal disbursed', 'user_account', ?)",
                     {std::to_string(user_id), std::to_string(-stored_amount),
                      std::to_string(rec_id)});
        (void)amount_cents;
        ok = true;
    });
    return ok;
}

int64_t SqlAdminExtraRepository::countUsers()
{
    std::vector<Row> rows = db_->query("SELECT COUNT(*) AS total FROM " + db_->table("users"), {});
    return rows.empty() ? 0 : rows.front().getInt("total");
}

int64_t SqlAdminExtraRepository::countOrders()
{
    std::vector<Row> rows = db_->query(
        "SELECT COUNT(*) AS total FROM " + db_->table("order_info"), {});
    return rows.empty() ? 0 : rows.front().getInt("total");
}

int64_t SqlAdminExtraRepository::countGoodsAll()
{
    std::vector<Row> rows = db_->query(
        "SELECT COUNT(*) AS total FROM " + db_->table("goods"), {});
    return rows.empty() ? 0 : rows.front().getInt("total");
}

int64_t SqlAdminExtraRepository::countRevenues()
{
    std::vector<Row> rows = db_->query(
        "SELECT COUNT(*) AS total FROM " + db_->table("order_info") +
            " WHERE order_status IN ('paid', 'shipped', 'received')",
        {});
    return rows.empty() ? 0 : rows.front().getInt("total");
}

} // namespace ecshop::infra
