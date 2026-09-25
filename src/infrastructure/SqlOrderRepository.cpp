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

} // namespace ecshop::infra
