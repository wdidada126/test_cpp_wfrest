#include "ecshop/infrastructure/SqlPaymentRepository.h"

namespace ecshop::infra {

domain::PaymentCallbackStatus SqlPaymentRepository::applyCallback(
    const std::string &provider, const std::string &provider_trade_no,
    const std::string &order_sn, int64_t amount_cents, const std::string &raw_payload)
{
    using domain::PaymentCallbackStatus;

    const std::string order_t = db_->table("order_info");
    const std::string pay_log_t = db_->table("pay_log");
    const std::string action_t = db_->table("order_action");
    bool sqlite = std::string(db_->driverName()) == "sqlite";

    PaymentCallbackStatus status = PaymentCallbackStatus::OrderNotFound;

    db_->transaction([&] {
        std::vector<Row> orders = db_->query(
            "SELECT order_id AS order_id, user_id AS user_id, order_amount AS order_amount,"
            " order_status AS order_status FROM " + order_t + " WHERE order_sn = ?",
            {order_sn});
        if (orders.empty())
            return;

        const Row &order = orders.front();
        int64_t order_id = order.getInt("order_id");
        int64_t user_id = order.getInt("user_id");

        int64_t expect_cents = 0;
        shared::Money::parse(shared::Money::normalize(order.get("order_amount")), expect_cents);

        // (provider, provider_trade_no) replay/conflict check first
        std::vector<Row> existing = db_->query(
            "SELECT order_id AS order_id, amount AS amount, raw_payload AS raw_payload FROM " +
                pay_log_t + " WHERE provider = ? AND provider_trade_no = ?",
            {provider, provider_trade_no});

        if (existing.empty())
        {
            if (order.get("order_status") != "pending_payment")
            {
                status = PaymentCallbackStatus::InvalidState;
                return;
            }
            if (amount_cents != expect_cents)
            {
                status = PaymentCallbackStatus::AmountMismatch;
                return;
            }

            int64_t flipped = db_->execute(
                "UPDATE " + order_t + " SET order_status = 'paid'" +
                    (std::string(!sqlite ? ", pay_time = " + db_->unixNow() : "")) +
                    " WHERE order_id = ? AND order_status = 'pending_payment'",
                {std::to_string(order_id)});
            if (flipped == 0)
            {
                status = PaymentCallbackStatus::InvalidState;
                return;
            }

            db_->execute("INSERT INTO " + pay_log_t +
                             " (order_id, provider, provider_trade_no, amount, status, raw_payload)"
                             " VALUES (?, ?, ?, ?, 'paid', ?)",
                         {std::to_string(order_id), provider, provider_trade_no,
                          shared::Money::format(amount_cents), raw_payload});

            if (sqlite)
            {
                db_->execute("INSERT INTO " + action_t +
                                 " (order_id, actor_user_id, action, note) VALUES (?, ?, 'paid', '')",
                             {std::to_string(order_id), std::to_string(user_id)});
            }
            else
            {
                db_->execute("INSERT INTO " + action_t +
                                 " (order_id, actor_type, actor_id, action_note)"
                                 " VALUES (?, 'payment', NULL, 'paid')",
                             {std::to_string(order_id)});
            }

            status = PaymentCallbackStatus::Completed;
            return;
        }

        // replayed or conflicting trade number
        int64_t logged_order = existing.front().getInt("order_id");
        int64_t logged_amount = 0;
        shared::Money::parse(shared::Money::normalize(existing.front().get("amount")),
                             logged_amount);
        if (logged_order == order_id && logged_amount == amount_cents)
            status = PaymentCallbackStatus::Replayed;
        else
            status = PaymentCallbackStatus::Conflict;
    });
    return status;
}

} // namespace ecshop::infra
