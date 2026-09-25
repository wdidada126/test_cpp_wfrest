#include "ecshop/infrastructure/SqlAdminOrderRepository.h"
#include "ecshop/shared/Money.h"
#include "ecshop/shared/TimeUtil.h"

namespace ecshop::infra {

static domain::OrderSummary rowToSummary(const Row &row)
{
    domain::OrderSummary summary;
    summary.order_id = row.getInt("order_id");
    summary.order_sn = row.get("order_sn");
    summary.status = row.get("order_status");
    summary.goods_amount = shared::Money::normalize(row.get("goods_amount"));
    summary.shipping_fee = shared::Money::normalize(row.get("shipping_fee"));
    summary.payment_fee = shared::Money::normalize(row.get("payment_fee"));
    summary.order_amount = shared::Money::normalize(row.get("order_amount"));
    summary.created_at = row.get("created_at");
    return summary;
}

static std::string summaryCols(const std::shared_ptr<Db> &db)
{
    return "order_id AS order_id, order_sn AS order_sn, order_status AS order_status,"
           " goods_amount AS goods_amount, shipping_fee AS shipping_fee,"
           " payment_fee AS payment_fee, order_amount AS order_amount," +
           db->toUnix("oi." + db->orderTimeCol()) + " AS created_at";
}

domain::OrderPage SqlAdminOrderRepository::list(int64_t offset, int64_t limit)
{
    const std::string order_t = db_->table("order_info");

    domain::OrderPage page;
    std::vector<Row> counts = db_->query("SELECT COUNT(*) AS total FROM " + order_t, {});
    if (!counts.empty())
        page.total = counts.front().getInt("total");

    // limit/offset are validated integers formatted by us; no user data here.
    std::string sql = "SELECT " + summaryCols(db_) + " FROM " + order_t +
                      " oi ORDER BY order_id DESC LIMIT " + std::to_string(limit) +
                      " OFFSET " + std::to_string(offset);

    for (const Row &row : db_->query(sql, {}))
        page.items.push_back(rowToSummary(row));
    return page;
}

std::optional<domain::OrderDetail> SqlAdminOrderRepository::find(int64_t order_id)
{
    const std::string order_t = db_->table("order_info");

    std::string sql = "SELECT " + summaryCols(db_) + ", consignee AS consignee,"
                                                 " address AS address, mobile AS mobile,"
                                                 " shipping_id AS shipping_id, pay_id AS pay_id,"
                                                 " remark AS remark FROM " +
                      order_t + " oi WHERE order_id = ?";

    std::vector<Row> rows = db_->query(sql, {std::to_string(order_id)});
    if (rows.empty())
        return std::nullopt;

    domain::OrderDetail detail;
    static_cast<domain::OrderSummary &>(detail) = rowToSummary(rows.front());
    detail.consignee = rows.front().get("consignee");
    detail.address = rows.front().get("address");
    detail.mobile = rows.front().get("mobile");
    detail.shipping_id = rows.front().getInt("shipping_id");
    detail.payment_id = rows.front().getInt("pay_id");
    detail.remark = rows.front().get("remark");

    std::string goods_sql =
        "SELECT goods_id AS goods_id, goods_name AS goods_name, goods_number AS goods_number,"
        " goods_price AS goods_price FROM " + db_->table("order_goods") +
        " WHERE order_id = ? ORDER BY rec_id";
    for (const Row &goods_row : db_->query(goods_sql, {std::to_string(order_id)}))
    {
        domain::OrderGoods goods;
        goods.goods_id = goods_row.getInt("goods_id");
        goods.name = goods_row.get("goods_name");
        goods.price = shared::Money::normalize(goods_row.get("goods_price"));
        goods.quantity = goods_row.getInt("goods_number");
        detail.items.push_back(std::move(goods));
    }
    return detail;
}

domain::OrderPatchStatus SqlAdminOrderRepository::ship(int64_t order_id)
{
    const std::string order_t = db_->table("order_info");
    const std::string action_t = db_->table("order_action");
    bool sqlite = std::string(db_->driverName()) == "sqlite";

    domain::OrderPatchStatus status = domain::OrderPatchStatus::NotFound;

    db_->transaction([&] {
        std::vector<Row> rows = db_->query(
            "SELECT order_id AS order_id FROM " + order_t + " WHERE order_id = ?",
            {std::to_string(order_id)});
        if (rows.empty())
            return;

        int64_t flipped = db_->execute(
            "UPDATE " + order_t + " SET order_status = 'shipped'" +
                std::string(sqlite ? "" : ", shipping_time = NOW()") +
                " WHERE order_id = ? AND order_status = 'paid'",
            {std::to_string(order_id)});
        if (flipped == 0)
        {
            status = domain::OrderPatchStatus::InvalidState;
            return;
        }

        if (sqlite)
        {
            db_->execute("INSERT INTO " + action_t +
                             " (order_id, actor_user_id, action, note)"
                             " VALUES (?, 0, 'shipped', 'admin')",
                         {std::to_string(order_id)});
        }
        else
        {
            db_->execute("INSERT INTO " + action_t +
                             " (order_id, actor_type, actor_id, action_note)"
                             " VALUES (?, 'admin', NULL, 'shipped')",
                         {std::to_string(order_id)});
        }

        status = domain::OrderPatchStatus::Ok;
    });
    return status;
}

} // namespace ecshop::infra
