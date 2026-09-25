#include "ecshop/infrastructure/SqlCheckoutRepository.h"
#include "ecshop/shared/Money.h"

namespace ecshop::infra {

using domain::PaymentOption;
using domain::ShippingOption;

std::vector<ShippingOption> SqlCheckoutRepository::listShipping()
{
    std::string sql =
        "SELECT shipping_id AS shipping_id, shipping_name AS shipping_name,"
        " shipping_fee AS shipping_fee FROM " + db_->table("shipping") +
        " WHERE enabled = 1 ORDER BY shipping_id";

    std::vector<ShippingOption> items;
    for (const Row &row : db_->query(sql, {}))
    {
        ShippingOption item;
        item.shipping_id = row.getInt("shipping_id");
        item.name = row.get("shipping_name");
        item.fee = shared::Money::normalize(row.get("shipping_fee"));
        items.push_back(std::move(item));
    }
    return items;
}

std::vector<PaymentOption> SqlCheckoutRepository::listPayment()
{
    std::string sql =
        "SELECT pay_id AS pay_id, pay_name AS pay_name, pay_fee AS pay_fee FROM " +
        db_->table("payment") + " WHERE enabled = 1 ORDER BY pay_id";

    std::vector<PaymentOption> items;
    for (const Row &row : db_->query(sql, {}))
    {
        PaymentOption item;
        item.payment_id = row.getInt("pay_id");
        item.name = row.get("pay_name");
        item.fee = shared::Money::normalize(row.get("pay_fee"));
        items.push_back(std::move(item));
    }
    return items;
}

std::optional<std::string> SqlCheckoutRepository::shippingFee(int64_t shipping_id)
{
    std::string sql = "SELECT shipping_fee AS shipping_fee FROM " + db_->table("shipping") +
                      " WHERE shipping_id = ? AND enabled = 1";
    std::vector<Row> rows = db_->query(sql, {std::to_string(shipping_id)});
    if (rows.empty())
        return std::nullopt;
    return shared::Money::normalize(rows.front().get("shipping_fee"));
}

std::optional<std::string> SqlCheckoutRepository::paymentFee(int64_t payment_id)
{
    std::string sql = "SELECT pay_fee AS pay_fee FROM " + db_->table("payment") +
                      " WHERE pay_id = ? AND enabled = 1";
    std::vector<Row> rows = db_->query(sql, {std::to_string(payment_id)});
    if (rows.empty())
        return std::nullopt;
    return shared::Money::normalize(rows.front().get("pay_fee"));
}

bool SqlCheckoutRepository::addressOwned(int64_t user_id, int64_t address_id)
{
    std::string sql = "SELECT address_id AS address_id FROM " + db_->table("user_address") +
                      " WHERE address_id = ? AND user_id = ?";
    return !db_->query(sql, {std::to_string(address_id), std::to_string(user_id)}).empty();
}

} // namespace ecshop::infra
