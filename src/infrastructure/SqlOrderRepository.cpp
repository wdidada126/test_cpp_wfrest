#include "ecshop/infrastructure/SqlOrderRepository.h"
#include "ecshop/shared/Money.h"
#include "ecshop/shared/Password.h"
#include "ecshop/shared/TimeUtil.h"

#include <cstdio>

namespace ecshop::infra {

using domain::OrderGoods;
using domain::OrderSummary;
using domain::PlaceOrderCommand;
using domain::PlaceOrderStatus;

std::string SqlOrderRepository::makeOrderSn()
{
    int64_t now = shared::nowUnix();
    std::string suffix = shared::randomTokenHex(3); // 6 hex chars
    char buf[48];
    std::snprintf(buf, sizeof(buf), "EC%lld%s", static_cast<long long>(now), suffix.c_str());
    return std::string(buf);
}

PlaceOrderStatus SqlOrderRepository::placeOrder(const PlaceOrderCommand &command,
                                                OrderSummary &out)
{
    std::string order_t = db_->table("order_info");
    std::string order_goods_t = db_->table("order_goods");
    std::string cart_t = db_->table("cart");
    std::string goods_t = db_->table("goods");
    std::string address_t = db_->table("user_address");

    PlaceOrderStatus status = PlaceOrderStatus::EmptyCart;

    db_->transaction([&] {
        // ---- idempotency: same user + key ----
        std::string order_time = db_->orderTimeCol();
        std::string summary_cols =
            "order_id AS order_id, order_sn AS order_sn, order_status AS order_status,"
            " goods_amount AS goods_amount, shipping_fee AS shipping_fee,"
            " payment_fee AS payment_fee, order_amount AS order_amount," +
            db_->toUnix(order_time) + " AS created_at, request_fingerprint AS request_fingerprint";

        std::vector<Row> existing = db_->query(
            "SELECT " + summary_cols + " FROM " + order_t +
                " WHERE user_id = ? AND idempotency_key = ?",
            {std::to_string(command.user_id), command.idempotency_key});
        if (!existing.empty())
        {
            const Row &row = existing.front();
            if (row.get("request_fingerprint") != command.fingerprint)
            {
                status = PlaceOrderStatus::KeyMismatch;
                return;
            }
            out.order_id = row.getInt("order_id");
            out.order_sn = row.get("order_sn");
            out.status = row.get("order_status");
            out.goods_amount = shared::Money::normalize(row.get("goods_amount"));
            out.shipping_fee = shared::Money::normalize(row.get("shipping_fee"));
            out.payment_fee = shared::Money::normalize(row.get("payment_fee"));
            out.order_amount = shared::Money::normalize(row.get("order_amount"));
            out.created_at = row.get("created_at");
            out.replayed = true;
            status = PlaceOrderStatus::Replayed;
            return;
        }

        // ---- delivery snapshot from the owned address ----
        std::vector<Row> addresses = db_->query(
            "SELECT consignee AS consignee, address AS address, mobile AS mobile FROM " +
                address_t + " WHERE address_id = ? AND user_id = ?",
            {std::to_string(command.address_id), std::to_string(command.user_id)});
        if (addresses.empty())
            return; // treated as EmptyCart-class failure by the route's pre-checks

        std::string consignee = addresses.front().get("consignee");
        std::string address_text = addresses.front().get("address");
        std::string mobile = addresses.front().get("mobile");

        // ---- cart rows with current prices ----
        std::vector<Row> cart_rows = db_->query(
            "SELECT c.goods_id AS goods_id, c.goods_number AS goods_number,"
            " g.goods_name AS goods_name, g.shop_price AS shop_price,"
            " g.market_price AS market_price FROM " + cart_t + " c JOIN " + goods_t +
                " g ON g.goods_id = c.goods_id"
                " WHERE c.user_id = ? AND g.is_on_sale = 1 AND g.is_delete = 0"
                " ORDER BY c.rec_id",
            {std::to_string(command.user_id)});
        if (cart_rows.empty())
        {
            status = PlaceOrderStatus::EmptyCart;
            return;
        }

        // ---- conditional stock deduction ----
        for (const Row &row : cart_rows)
        {
            int64_t need = row.getInt("goods_number");
            int64_t changed = db_->execute(
                "UPDATE " + goods_t + " SET goods_number = goods_number - ?" +
                    " WHERE goods_id = ? AND goods_number >= ?",
                {std::to_string(need), std::to_string(row.getInt("goods_id")),
                 std::to_string(need)});
            if (changed == 0)
            {
                status = PlaceOrderStatus::OutOfStock;
                return;
            }
        }

        // ---- totals ----
        int64_t goods_cents = 0;
        for (const Row &row : cart_rows)
        {
            int64_t unit_cents = 0;
            shared::Money::parse(shared::Money::normalize(row.get("shop_price")), unit_cents);
            goods_cents += unit_cents * row.getInt("goods_number");
        }

        int64_t shipping_cents = 0, payment_cents = 0;
        std::vector<Row> shippings = db_->query(
            "SELECT shipping_fee AS shipping_fee FROM " + db_->table("shipping") +
                " WHERE shipping_id = ?",
            {std::to_string(command.shipping_id)});
        if (!shippings.empty())
            shared::Money::parse(shared::Money::normalize(shippings.front().get("shipping_fee")),
                                 shipping_cents);
        std::vector<Row> payments = db_->query(
            "SELECT pay_fee AS pay_fee FROM " + db_->table("payment") + " WHERE pay_id = ?",
            {std::to_string(command.payment_id)});
        if (!payments.empty())
            shared::Money::parse(shared::Money::normalize(payments.front().get("pay_fee")),
                                 payment_cents);

        int64_t order_cents = goods_cents + shipping_cents + payment_cents;

        // ---- order row ----
        std::string order_sn = makeOrderSn();
        db_->execute("INSERT INTO " + order_t +
                         " (order_sn, user_id, order_status, consignee, address, mobile,"
                         " shipping_id, pay_id, goods_amount, shipping_fee, payment_fee,"
                         " order_amount, idempotency_key, request_fingerprint, remark)"
                         " VALUES (?, ?, 'pending_payment', ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)",
                     {order_sn, std::to_string(command.user_id), consignee, address_text, mobile,
                      std::to_string(command.shipping_id), std::to_string(command.payment_id),
                      shared::Money::format(goods_cents), shared::Money::format(shipping_cents),
                      shared::Money::format(payment_cents), shared::Money::format(order_cents),
                      command.idempotency_key, command.fingerprint, command.remark});
        int64_t order_id = db_->lastInsertId();

        // ---- goods snapshot (column sets differ per backend) ----
        bool sqlite = std::string(db_->driverName()) == "sqlite";
        for (const Row &row : cart_rows)
        {
            int64_t unit_cents = 0;
            shared::Money::parse(shared::Money::normalize(row.get("shop_price")), unit_cents);

            if (sqlite)
            {
                db_->execute("INSERT INTO " + order_goods_t +
                                 " (order_id, goods_id, goods_name, goods_number, goods_price)"
                                 " VALUES (?, ?, ?, ?, ?)",
                             {std::to_string(order_id), std::to_string(row.getInt("goods_id")),
                              row.get("goods_name"), std::to_string(row.getInt("goods_number")),
                              shared::Money::format(unit_cents)});
            }
            else
            {
                // mysql schema additionally requires market_price and goods_attr
                db_->execute("INSERT INTO " + order_goods_t +
                                 " (order_id, goods_id, goods_sn, goods_name, goods_number,"
                                 " market_price, goods_price, goods_attr)"
                                 " VALUES (?, ?, '', ?, ?, ?, ?, '')",
                             {std::to_string(order_id), std::to_string(row.getInt("goods_id")),
                              row.get("goods_name"), std::to_string(row.getInt("goods_number")),
                              shared::Money::normalize(row.get("market_price")),
                              shared::Money::format(unit_cents)});
            }
        }

        // ---- clear the cart ----
        db_->execute("DELETE FROM " + cart_t + " WHERE user_id = ?",
                     {std::to_string(command.user_id)});

        out.order_id = order_id;
        out.order_sn = order_sn;
        out.status = "pending_payment";
        out.goods_amount = shared::Money::format(goods_cents);
        out.shipping_fee = shared::Money::format(shipping_cents);
        out.payment_fee = shared::Money::format(payment_cents);
        out.order_amount = shared::Money::format(order_cents);
        out.created_at = std::to_string(shared::nowUnix());
        out.replayed = false;
        status = PlaceOrderStatus::Created;
    });
    return status;
}

static OrderSummary rowToOrderSummary(const Row &row)
{
    OrderSummary summary;
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

domain::OrderPage SqlOrderRepository::listOfUser(int64_t user_id, int64_t offset, int64_t limit)
{
    const std::string order_t = db_->table("order_info");
    const std::string order_time = db_->orderTimeCol();

    domain::OrderPage page;
    std::vector<Row> counts = db_->query(
        "SELECT COUNT(*) AS total FROM " + order_t + " WHERE user_id = ?",
        {std::to_string(user_id)});
    if (!counts.empty())
        page.total = counts.front().getInt("total");

    // limit/offset are validated integers formatted by us; user data stays bound.
    std::string sql =
        "SELECT order_id AS order_id, order_sn AS order_sn, order_status AS order_status,"
        " goods_amount AS goods_amount, shipping_fee AS shipping_fee,"
        " payment_fee AS payment_fee, order_amount AS order_amount," +
        db_->toUnix(order_time) + " AS created_at FROM " + order_t +
        " WHERE user_id = ? ORDER BY order_id DESC LIMIT " + std::to_string(limit) +
        " OFFSET " + std::to_string(offset);

    for (const Row &row : db_->query(sql, {std::to_string(user_id)}))
        page.items.push_back(rowToOrderSummary(row));
    return page;
}

std::optional<domain::OrderDetail> SqlOrderRepository::findOfUser(int64_t user_id,
                                                                  int64_t order_id)
{
    const std::string order_t = db_->table("order_info");
    const std::string order_time = db_->orderTimeCol();

    std::string sql =
        "SELECT order_id AS order_id, order_sn AS order_sn, order_status AS order_status,"
        " goods_amount AS goods_amount, shipping_fee AS shipping_fee,"
        " payment_fee AS payment_fee, order_amount AS order_amount,"
        " consignee AS consignee, address AS address, mobile AS mobile,"
        " shipping_id AS shipping_id, pay_id AS pay_id, remark AS remark," +
        db_->toUnix(order_time) + " AS created_at FROM " + order_t +
        " WHERE order_id = ? AND user_id = ?";

    std::vector<Row> rows = db_->query(sql, {std::to_string(order_id), std::to_string(user_id)});
    if (rows.empty())
        return std::nullopt;

    const Row &row = rows.front();
    // OrderDetail derives from OrderSummary: a base-class value cannot be
    // copy-initialized into a derived object. Assign the base subobject.
    domain::OrderDetail detail;
    static_cast<domain::OrderSummary &>(detail) = rowToOrderSummary(row);
    detail.consignee = row.get("consignee");
    detail.address = row.get("address");
    detail.mobile = row.get("mobile");
    detail.shipping_id = row.getInt("shipping_id");
    detail.payment_id = row.getInt("pay_id");
    detail.remark = row.get("remark");

    // goods snapshot columns shared by both schemas
    std::string goods_sql =
        "SELECT goods_id AS goods_id, goods_name AS goods_name,"
        " goods_number AS goods_number, goods_price AS goods_price FROM " +
        db_->table("order_goods") + " WHERE order_id = ? ORDER BY rec_id";
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

domain::OrderCancelStatus SqlOrderRepository::cancelOfUser(int64_t user_id, int64_t order_id,
                                                           OrderSummary &out)
{
    const std::string order_t = db_->table("order_info");
    const std::string order_goods_t = db_->table("order_goods");
    const std::string goods_t = db_->table("goods");
    const std::string balance_t = db_->table("account_balance");
    const std::string log_t = db_->table("account_log");
    const std::string action_t = db_->table("order_action");
    const std::string balance_payment_t = db_->table("order_balance_payment");
    bool sqlite = std::string(db_->driverName()) == "sqlite";

    domain::OrderCancelStatus status = domain::OrderCancelStatus::NotFound;

    db_->transaction([&] {
        // only the owner's pending_payment order can be cancelled
        std::string order_time = db_->orderTimeCol();
        std::string summary_cols =
            "order_id AS order_id, order_sn AS order_sn, order_status AS order_status,"
            " goods_amount AS goods_amount, shipping_fee AS shipping_fee,"
            " payment_fee AS payment_fee, order_amount AS order_amount," +
            db_->toUnix(order_time) + " AS created_at";

        std::vector<Row> rows = db_->query(
            "SELECT " + summary_cols + " FROM " + order_t +
                " WHERE order_id = ? AND user_id = ?",
            {std::to_string(order_id), std::to_string(user_id)});
        if (rows.empty())
            return;

        // conditional status flip: concurrent cancels cannot double-restock
        int64_t flipped = db_->execute(
            "UPDATE " + order_t +
                " SET order_status = 'cancelled' WHERE order_id = ? AND user_id = ?"
                " AND order_status = 'pending_payment'",
            {std::to_string(order_id), std::to_string(user_id)});
        if (flipped == 0)
        {
            status = domain::OrderCancelStatus::InvalidState;
            return;
        }

        // restock the snapshot quantities
        std::vector<Row> snapshot = db_->query(
            "SELECT goods_id AS goods_id, goods_number AS goods_number FROM " + order_goods_t +
                " WHERE order_id = ?",
            {std::to_string(order_id)});
        for (const Row &item : snapshot)
        {
            db_->execute("UPDATE " + goods_t + " SET goods_number = goods_number + ?" +
                             " WHERE goods_id = ?",
                         {std::to_string(item.getInt("goods_number")),
                          std::to_string(item.getInt("goods_id"))});
        }

        // refund any balance paid for this order
        std::vector<Row> paid = db_->query(
            "SELECT paid_cents AS paid_cents FROM " + balance_payment_t + " WHERE order_id = ?",
            {std::to_string(order_id)});
        if (!paid.empty() && paid.front().getInt("paid_cents") > 0)
        {
            int64_t paid_cents = paid.front().getInt("paid_cents");
            db_->execute("UPDATE " + balance_t +
                             " SET available_cents = available_cents + ? WHERE user_id = ?",
                         {std::to_string(paid_cents), std::to_string(user_id)});
            db_->execute("INSERT INTO " + log_t +
                             " (user_id, available_delta_cents, frozen_delta_cents, reason,"
                             " reference_type, reference_id)"
                             " VALUES (?, ?, 0, 'order cancelled refund', 'order_info', ?)",
                         {std::to_string(user_id), std::to_string(paid_cents),
                          std::to_string(order_id)});
            db_->execute("DELETE FROM " + balance_payment_t + " WHERE order_id = ?",
                         {std::to_string(order_id)});
        }

        // order audit (column sets differ per backend)
        if (sqlite)
        {
            db_->execute("INSERT INTO " + action_t +
                             " (order_id, actor_user_id, action, note) VALUES (?, ?, 'cancel', '')",
                         {std::to_string(order_id), std::to_string(user_id)});
        }
        else
        {
            db_->execute("INSERT INTO " + action_t +
                             " (order_id, actor_type, actor_id, action_note)"
                             " VALUES (?, 'user', ?, 'cancel')",
                         {std::to_string(order_id), std::to_string(user_id)});
        }

        out = rowToOrderSummary(rows.front());
        out.status = "cancelled";
        status = domain::OrderCancelStatus::Cancelled;
    });
    return status;
}

domain::OrderCancelStatus SqlOrderRepository::receivedOfUser(int64_t user_id, int64_t order_id,
                                                             OrderSummary &out)
{
    const std::string order_t = db_->table("order_info");
    const std::string action_t = db_->table("order_action");
    bool sqlite = std::string(db_->driverName()) == "sqlite";

    domain::OrderCancelStatus status = domain::OrderCancelStatus::NotFound;

    db_->transaction([&] {
        std::string order_time = db_->orderTimeCol();
        std::string summary_cols =
            "order_id AS order_id, order_sn AS order_sn, order_status AS order_status,"
            " goods_amount AS goods_amount, shipping_fee AS shipping_fee,"
            " payment_fee AS payment_fee, order_amount AS order_amount," +
            db_->toUnix(order_time) + " AS created_at";

        std::vector<Row> rows = db_->query(
            "SELECT " + summary_cols + " FROM " + order_t +
                " WHERE order_id = ? AND user_id = ?",
            {std::to_string(order_id), std::to_string(user_id)});
        if (rows.empty())
            return;

        int64_t flipped = db_->execute(
            "UPDATE " + order_t +
                " SET order_status = 'received' WHERE order_id = ? AND user_id = ?"
                " AND order_status = 'paid'",
            {std::to_string(order_id), std::to_string(user_id)});
        if (flipped == 0)
        {
            status = domain::OrderCancelStatus::InvalidState;
            return;
        }

        if (sqlite)
        {
            db_->execute("INSERT INTO " + action_t +
                             " (order_id, actor_user_id, action, note)"
                             " VALUES (?, ?, 'received', '')",
                         {std::to_string(order_id), std::to_string(user_id)});
        }
        else
        {
            db_->execute("INSERT INTO " + action_t +
                             " (order_id, actor_type, actor_id, action_note)"
                             " VALUES (?, 'user', ?, 'received')",
                         {std::to_string(order_id), std::to_string(user_id)});
        }

        out = rowToOrderSummary(rows.front());
        out.status = "received";
        status = domain::OrderCancelStatus::Cancelled;
    });
    return status;
}

domain::OrderReturnStatus SqlOrderRepository::returnToCart(int64_t user_id, int64_t order_id)
{
    const std::string order_t = db_->table("order_info");
    const std::string order_goods_t = db_->table("order_goods");
    const std::string goods_t = db_->table("goods");
    const std::string cart_t = db_->table("cart");

    domain::OrderReturnStatus status = domain::OrderReturnStatus::NotFound;

    db_->transaction([&] {
        std::vector<Row> orders = db_->query(
            "SELECT order_id AS order_id FROM " + order_t +
                " WHERE order_id = ? AND user_id = ?",
            {std::to_string(order_id), std::to_string(user_id)});
        if (orders.empty())
            return;

        bool any = false;
        std::vector<Row> snapshot = db_->query(
            "SELECT goods_id AS goods_id, goods_number AS goods_number FROM " + order_goods_t +
                " WHERE order_id = ?",
            {std::to_string(order_id)});
        for (const Row &item : snapshot)
        {
            // cap at current saleable stock
            std::vector<Row> goods = db_->query(
                "SELECT goods_number AS goods_number FROM " + goods_t +
                    " WHERE goods_id = ? AND is_on_sale = 1 AND is_delete = 0",
                {std::to_string(item.getInt("goods_id"))});
            if (goods.empty() || goods.front().getInt("goods_number") <= 0)
                continue;

            int64_t want = item.getInt("goods_number");
            int64_t available = goods.front().getInt("goods_number");
            int64_t quantity = want < available ? want : available;

            // portable upsert: accumulate if the row exists, insert otherwise
            int64_t changed = db_->execute(
                "UPDATE " + cart_t +
                    " SET goods_number = goods_number + ?, version = version + 1"
                    " WHERE user_id = ? AND goods_id = ?",
                {std::to_string(quantity), std::to_string(user_id),
                 std::to_string(item.getInt("goods_id"))});
            if (changed == 0)
            {
                try
                {
                    db_->execute("INSERT INTO " + cart_t +
                                     " (user_id, goods_id, goods_number, version)"
                                     " VALUES (?, ?, ?, 1)",
                                 {std::to_string(user_id),
                                  std::to_string(item.getInt("goods_id")),
                                  std::to_string(quantity)});
                }
                catch (const DbError &)
                {
                    db_->execute("UPDATE " + cart_t +
                                     " SET goods_number = goods_number + ?, version = version + 1"
                                     " WHERE user_id = ? AND goods_id = ?",
                                 {std::to_string(quantity), std::to_string(user_id),
                                  std::to_string(item.getInt("goods_id"))});
                }
            }
            any = true;
        }

        status = any ? domain::OrderReturnStatus::Ok : domain::OrderReturnStatus::NothingToReturn;
    });
    return status;
}

std::optional<domain::OrderSummary> SqlOrderRepository::findBySnOfUser(int64_t user_id,
                                                                        const std::string &order_sn)
{
    std::string order_time = db_->orderTimeCol();
    std::string sql =
        "SELECT order_id AS order_id, order_sn AS order_sn, order_status AS order_status,"
        " goods_amount AS goods_amount, shipping_fee AS shipping_fee,"
        " payment_fee AS payment_fee, order_amount AS order_amount," +
        db_->toUnix(order_time) + " AS created_at FROM " + db_->table("order_info") +
        " WHERE order_sn = ? AND user_id = ?";

    std::vector<Row> rows = db_->query(sql, {order_sn, std::to_string(user_id)});
    if (rows.empty())
        return std::nullopt;
    return rowToOrderSummary(rows.front());
}

domain::OrderPatchStatus SqlOrderRepository::updateAddressOfUser(
    int64_t user_id, int64_t order_id, const domain::DeliveryAddressPatch &patch)
{
    const std::string order_t = db_->table("order_info");
    const std::string detail_t = db_->table("order_delivery_address");
    const std::string action_t = db_->table("order_action");
    bool sqlite = std::string(db_->driverName()) == "sqlite";

    domain::OrderPatchStatus status = domain::OrderPatchStatus::NotFound;

    db_->transaction([&] {
        std::vector<Row> rows = db_->query(
            "SELECT order_id AS order_id, order_status AS order_status FROM " + order_t +
                " WHERE order_id = ? AND user_id = ?",
            {std::to_string(order_id), std::to_string(user_id)});
        if (rows.empty())
            return;

        int64_t changed = db_->execute(
            "UPDATE " + order_t +
                " SET consignee = ?, address = ?, mobile = ? WHERE order_id = ? AND user_id = ?" +
                std::string(" AND order_status = 'pending_payment'") +
                (sqlite ? "" : " AND shipping_status = 'unshipped'"),
            {patch.consignee, patch.address, patch.mobile, std::to_string(order_id),
             std::to_string(user_id)});
        if (changed == 0)
        {
            status = domain::OrderPatchStatus::InvalidState;
            return;
        }

        // one-to-one delivery detail: replace, not merge
        db_->execute("DELETE FROM " + detail_t + " WHERE order_id = ?",
                     {std::to_string(order_id)});
        db_->execute("INSERT INTO " + detail_t +
                         " (order_id, user_id, email, zipcode, telephone, sign_building,"
                         " best_time) VALUES (?, ?, ?, ?, ?, ?, ?)",
                     {std::to_string(order_id), std::to_string(user_id), patch.email,
                      patch.zipcode, patch.tel, patch.sign_building, patch.best_time});

        if (sqlite)
        {
            db_->execute("INSERT INTO " + action_t +
                             " (order_id, actor_user_id, action, note)"
                             " VALUES (?, ?, 'edit_address', '')",
                         {std::to_string(order_id), std::to_string(user_id)});
        }
        else
        {
            db_->execute("INSERT INTO " + action_t +
                             " (order_id, actor_type, actor_id, action_note)"
                             " VALUES (?, 'user', ?, 'edit_address')",
                         {std::to_string(order_id), std::to_string(user_id)});
        }

        status = domain::OrderPatchStatus::Ok;
    });
    return status;
}

domain::OrderPatchStatus SqlOrderRepository::updatePaymentOfUser(int64_t user_id,
                                                                 int64_t order_id,
                                                                 int64_t payment_id)
{
    const std::string order_t = db_->table("order_info");
    const std::string payment_t = db_->table("payment");
    const std::string action_t = db_->table("order_action");
    bool sqlite = std::string(db_->driverName()) == "sqlite";

    domain::OrderPatchStatus status = domain::OrderPatchStatus::NotFound;

    db_->transaction([&] {
        std::vector<Row> rows = db_->query(
            "SELECT pay_id AS pay_id, order_amount AS order_amount FROM " + order_t +
                " WHERE order_id = ? AND user_id = ? AND order_status = 'pending_payment'" +
                std::string(sqlite ? "" : " AND shipping_status = 'unshipped'"),
            {std::to_string(order_id), std::to_string(user_id)});
        if (rows.empty())
            return;

        int64_t old_payment_id = rows.front().getInt("pay_id");
        if (old_payment_id == payment_id)
        {
            status = domain::OrderPatchStatus::InvalidState; // unchanged method
            return;
        }

        std::vector<Row> payments = db_->query(
            "SELECT pay_fee AS pay_fee FROM " + payment_t + " WHERE pay_id = ? AND enabled = 1",
            {std::to_string(payment_id)});
        if (payments.empty())
        {
            status = domain::OrderPatchStatus::NotFound; // disabled/unknown method
            return;
        }

        std::vector<Row> old_fees = db_->query(
            "SELECT pay_fee AS pay_fee FROM " + payment_t + " WHERE pay_id = ?",
            {std::to_string(old_payment_id)});
        int64_t old_fee_cents = 0;
        if (!old_fees.empty())
            shared::Money::parse(shared::Money::normalize(old_fees.front().get("pay_fee")),
                                 old_fee_cents);

        int64_t order_cents = 0;
        shared::Money::parse(shared::Money::normalize(rows.front().get("order_amount")),
                             order_cents);
        int64_t new_fee_cents = 0;
        shared::Money::parse(shared::Money::normalize(payments.front().get("pay_fee")),
                             new_fee_cents);

        int64_t goods_unpaid_cents = order_cents - old_fee_cents;
        if (goods_unpaid_cents < 0)
            goods_unpaid_cents = 0;
        int64_t new_order_cents = goods_unpaid_cents + new_fee_cents;

        // conditional flip; a concurrent status change rolls everything back
        int64_t changed = db_->execute(
            "UPDATE " + order_t + " SET pay_id = ?, payment_fee = ?, order_amount = ?" +
                std::string(" WHERE order_id = ? AND user_id = ?") +
                std::string(" AND order_status = 'pending_payment'") +
                std::string(sqlite ? "" : " AND shipping_status = 'unshipped'"),
            {std::to_string(payment_id), shared::Money::format(new_fee_cents),
             shared::Money::format(new_order_cents), std::to_string(order_id),
             std::to_string(user_id)});
        if (changed == 0)
        {
            status = domain::OrderPatchStatus::NotFound; // state changed concurrently
            return;
        }

        if (sqlite)
        {
            db_->execute("INSERT INTO " + action_t +
                             " (order_id, actor_user_id, action, note)"
                             " VALUES (?, ?, 'edit_payment', '')",
                         {std::to_string(order_id), std::to_string(user_id)});
        }
        else
        {
            db_->execute("INSERT INTO " + action_t +
                             " (order_id, actor_type, actor_id, action_note)"
                             " VALUES (?, 'user', ?, 'edit_payment')",
                         {std::to_string(order_id), std::to_string(user_id)});
        }

        status = domain::OrderPatchStatus::Ok;
    });
    return status;
}

domain::SurplusResult SqlOrderRepository::payWithSurplus(int64_t user_id, int64_t order_id,
                                                         int64_t amount_cents)
{
    using domain::OrderSurplusStatus;
    using domain::SurplusResult;

    const std::string order_t = db_->table("order_info");
    const std::string payment_t = db_->table("payment");
    const std::string balance_t = db_->table("account_balance");
    const std::string balance_pay_t = db_->table("order_balance_payment");
    const std::string log_t = db_->table("account_log");
    const std::string action_t = db_->table("order_action");
    bool sqlite = std::string(db_->driverName()) == "sqlite";

    SurplusResult result;

    db_->transaction([&] {
        std::vector<Row> rows = db_->query(
            "SELECT order_amount AS order_amount, payment_fee AS payment_fee"
            " FROM " + order_t + " WHERE order_id = ? AND user_id = ? AND order_status = 'pending_payment'",
            {std::to_string(order_id), std::to_string(user_id)});
        if (rows.empty())
            return;

        int64_t order_cents = 0;
        shared::Money::parse(shared::Money::normalize(rows.front().get("order_amount")),
                             order_cents);
        int64_t fee_cents = 0;
        shared::Money::parse(shared::Money::normalize(rows.front().get("payment_fee")),
                             fee_cents);

        // remaining payable excludes the fixed payment fee
        int64_t payable_cents = order_cents - fee_cents;
        if (payable_cents < 0)
            payable_cents = 0;

        int64_t paid_cents = 0;
        std::vector<Row> paid = db_->query(
            "SELECT paid_cents AS paid_cents FROM " + balance_pay_t + " WHERE order_id = ?",
            {std::to_string(order_id)});
        if (!paid.empty())
            paid_cents = paid.front().getInt("paid_cents");

        int64_t remaining_cents = payable_cents - paid_cents;
        if (remaining_cents <= 0)
        {
            result.status = domain::OrderSurplusStatus::InvalidState;
            return;
        }

        int64_t applied_cents = amount_cents < remaining_cents ? amount_cents : remaining_cents;

        // conditional balance deduction against funds available
        int64_t balance_changed = db_->execute(
            "UPDATE " + balance_t + " SET available_cents = available_cents - ?" +
                " WHERE user_id = ? AND available_cents >= ?",
            {std::to_string(applied_cents), std::to_string(user_id),
             std::to_string(applied_cents)});
        if (balance_changed == 0)
        {
            result.status = domain::OrderSurplusStatus::InvalidState; // insufficient
            return;
        }

        int64_t paid_total_cents = paid_cents + applied_cents;

        // accumulate the standalone balance payment table
        int64_t upsert_changed = db_->execute(
            "UPDATE " + balance_pay_t + " SET paid_cents = ?, updated_at = ? WHERE order_id = ?",
            {std::to_string(paid_total_cents), db_->datetimeFromUnix(shared::nowUnix()),
             std::to_string(order_id)});
        if (upsert_changed == 0)
        {
            db_->execute("INSERT INTO " + balance_pay_t +
                             " (order_id, user_id, paid_cents, updated_at) VALUES (?, ?, ?, ?)",
                         {std::to_string(order_id), std::to_string(user_id),
                          std::to_string(paid_total_cents),
                          db_->datetimeFromUnix(shared::nowUnix())});
        }

        db_->execute("INSERT INTO " + log_t +
                         " (user_id, available_delta_cents, frozen_delta_cents, reason,"
                         " reference_type, reference_id)"
                         " VALUES (?, ?, 0, 'order surplus payment', 'order_info', ?)",
                     {std::to_string(user_id), std::to_string(-applied_cents),
                      std::to_string(order_id)});

        bool became_paid = paid_total_cents >= payable_cents;
        if (became_paid)
        {
            int64_t flipped = db_->execute(
                "UPDATE " + order_t + " SET order_status = 'paid', pay_time = " +
                    db_->unixNow() +
                    " WHERE order_id = ? AND order_status = 'pending_payment'",
                {std::to_string(order_id)});
            if (flipped == 0)
            {
                result.status = domain::OrderSurplusStatus::InvalidState;
                return;
            }
        }

        if (sqlite)
        {
            db_->execute("INSERT INTO " + action_t +
                             " (order_id, actor_user_id, action, note) VALUES (?, ?, 'surplus', '')",
                         {std::to_string(order_id), std::to_string(user_id)});
        }
        else
        {
            db_->execute("INSERT INTO " + action_t +
                             " (order_id, actor_type, actor_id, action_note)"
                             " VALUES (?, 'user', ?, 'edit_surplus')",
                         {std::to_string(order_id), std::to_string(user_id)});
        }

        result.status = became_paid ? domain::OrderSurplusStatus::FullyPaid
                                    : domain::OrderSurplusStatus::PartiallyPaid;
        result.applied = shared::Money::format(applied_cents);
        result.remaining = shared::Money::format(payable_cents - paid_total_cents);
        result.paid_total = shared::Money::format(paid_total_cents);
        result.became_paid = became_paid;
    });
    return result;
}

domain::OrderMergeStatus SqlOrderRepository::mergeOrders(int64_t user_id, int64_t from_order_id,
                                                         int64_t to_order_id, OrderSummary &out)
{
    using domain::OrderMergeStatus;

    if (from_order_id == to_order_id)
        return domain::OrderMergeStatus::InvalidState;

    const std::string order_t = db_->table("order_info");
    const std::string order_goods_t = db_->table("order_goods");
    const std::string balance_pay_t = db_->table("order_balance_payment");
    const std::string action_t = db_->table("order_action");
    bool sqlite = std::string(db_->driverName()) == "sqlite";

    OrderMergeStatus status = OrderMergeStatus::NotFound;

    db_->transaction([&] {
        // both must be own pending_payment orders
        std::vector<Row> orders = db_->query(
            "SELECT order_id AS order_id, consignee AS consignee, address AS address,"
            " mobile AS mobile, shipping_id AS shipping_id, pay_id AS pay_id,"
            " goods_amount AS goods_amount, shipping_fee AS shipping_fee,"
            " payment_fee AS payment_fee FROM " + order_t +
                " WHERE user_id = ? AND order_status = 'pending_payment'"
                " AND order_id IN (?, ?)",
            {std::to_string(user_id), std::to_string(from_order_id),
             std::to_string(to_order_id)});
        if (orders.size() != 2)
            return;

        // merging requires unused account balance on both sides
        for (int64_t id : {from_order_id, to_order_id})
        {
            std::vector<Row> paid = db_->query(
                "SELECT paid_cents AS paid_cents FROM " + balance_pay_t +
                    " WHERE order_id = ?",
                {std::to_string(id)});
            if (!paid.empty() && paid.front().getInt("paid_cents") > 0)
            {
                status = OrderMergeStatus::InvalidState;
                return;
            }
        }

        const Row *from_row = &orders.front();
        const Row *to_row = &orders.back();

        int64_t goods_cents = 0, shipping_cents = 0, payment_cents = 0;
        for (const Row *row : {from_row, to_row})
        {
            int64_t v = 0;
            std::string col_cost = row->get("goods_amount");
            shared::Money::parse(shared::Money::normalize(col_cost), v);
            goods_cents += v;
            shared::Money::parse(shared::Money::normalize(row->get("shipping_fee")), v);
            shipping_cents += v;
            shared::Money::parse(shared::Money::normalize(row->get("payment_fee")), v);
            payment_cents += v;
        }

        // new merged order from the "to" delivery snapshot
        std::string order_sn = makeOrderSn();
        db_->execute(
            "INSERT INTO " + order_t +
                " (order_sn, user_id, order_status, consignee, address, mobile,"
                " shipping_id, pay_id, goods_amount, shipping_fee, payment_fee,"
                " order_amount, idempotency_key, request_fingerprint, remark)"
                " VALUES (?, ?, 'pending_payment', ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, '')",
            {order_sn, std::to_string(user_id), to_row->get("consignee"),
             to_row->get("address"), to_row->get("mobile"),
             to_row->get("shipping_id"), to_row->get("pay_id"),
             shared::Money::format(goods_cents), shared::Money::format(shipping_cents),
             shared::Money::format(payment_cents),
             shared::Money::format(goods_cents + shipping_cents + payment_cents),
             "merged-" + order_sn, "merged-" + order_sn});
        int64_t merged_id = db_->lastInsertId();

        // move both goods snapshots onto the merged order
        for (int64_t old_id : {from_order_id, to_order_id})
        {
            std::vector<Row> snapshot = db_->query(
                "SELECT goods_id AS goods_id, goods_name AS goods_name,"
                " goods_number AS goods_number, goods_price AS goods_price FROM " +
                    order_goods_t + " WHERE order_id = ?",
                {std::to_string(old_id)});
            for (const Row &item : snapshot)
            {
                if (sqlite)
                {
                    db_->execute("INSERT INTO " + order_goods_t +
                                     " (order_id, goods_id, goods_name, goods_number,"
                                     " goods_price) VALUES (?, ?, ?, ?, ?)",
                                 {std::to_string(merged_id),
                                  std::to_string(item.getInt("goods_id")),
                                  item.get("goods_name"),
                                  std::to_string(item.getInt("goods_number")),
                                  item.get("goods_price")});
                }
                else
                {
                    db_->execute("INSERT INTO " + order_goods_t +
                                     " (order_id, goods_id, goods_name, goods_number,"
                                     " market_price, goods_price, goods_attr)"
                                     " VALUES (?, ?, '', ?, ?, ?, 'merged', '')",
                                 {std::to_string(merged_id),
                                  std::to_string(item.getInt("goods_id")),
                                  item.get("goods_name"),
                                  std::to_string(item.getInt("goods_number")),
                                  item.get("goods_price"), item.get("goods_price")});
                }
            }
        }

        // old audit rows are cleaned up inside the same transaction
        db_->execute("DELETE FROM " + action_t +
                         " WHERE order_id IN (?, ?)",
                     {std::to_string(from_order_id), std::to_string(to_order_id)});

        // mark both old orders so they can no longer be paid
        for (int64_t old_id : {from_order_id, to_order_id})
        {
            db_->execute("UPDATE " + order_t + " SET order_status = 'merged' WHERE order_id = ?",
                         {std::to_string(old_id)});
        }

        if (sqlite)
        {
            db_->execute("INSERT INTO " + action_t +
                             " (order_id, actor_user_id, action, note)"
                             " VALUES (?, ?, 'merge', '')",
                         {std::to_string(merged_id), std::to_string(user_id)});
        }
        else
        {
            db_->execute("INSERT INTO " + action_t +
                             " (order_id, actor_type, actor_id, action_note)"
                             " VALUES (?, 'user', ?, 'merge')",
                         {std::to_string(merged_id), std::to_string(user_id)});
        }

        out.order_id = merged_id;
        out.order_sn = order_sn;
        out.status = "pending_payment";
        out.goods_amount = shared::Money::format(goods_cents);
        out.shipping_fee = shared::Money::format(shipping_cents);
        out.payment_fee = shared::Money::format(payment_cents);
        out.order_amount = shared::Money::format(goods_cents + shipping_cents + payment_cents);
        out.created_at = std::to_string(shared::nowUnix());
        out.replayed = false;
        status = OrderMergeStatus::Merged;
    });
    return status;
}

} // namespace ecshop::infra
