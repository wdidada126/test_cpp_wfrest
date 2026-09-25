#include "ecshop/infrastructure/SqlAdminPaymentRepository.h"
#include "ecshop/shared/Money.h"

namespace ecshop::infra {

std::vector<domain::PaymentOption> SqlAdminPaymentRepository::listAll()
{
    std::string sql = "SELECT pay_id AS pay_id, pay_name AS pay_name, pay_fee AS pay_fee FROM " +
                      db_->table("payment") + " ORDER BY pay_id";
    std::vector<domain::PaymentOption> items;
    for (const Row &row : db_->query(sql, {}))
    {
        domain::PaymentOption item;
        item.payment_id = row.getInt("pay_id");
        item.name = row.get("pay_name");
        item.fee = shared::Money::normalize(row.get("pay_fee"));
        items.push_back(std::move(item));
    }
    return items;
}

std::optional<domain::PaymentOption> SqlAdminPaymentRepository::find(int64_t pay_id)
{
    std::string sql = "SELECT pay_id AS pay_id, pay_name AS pay_name, pay_fee AS pay_fee FROM " +
                      db_->table("payment") + " WHERE pay_id = ?";
    std::vector<Row> rows = db_->query(sql, {std::to_string(pay_id)});
    if (rows.empty())
        return std::nullopt;
    domain::PaymentOption item;
    item.payment_id = rows.front().getInt("pay_id");
    item.name = rows.front().get("pay_name");
    item.fee = shared::Money::normalize(rows.front().get("pay_fee"));
    return item;
}

void SqlAdminPaymentRepository::create(const std::string &name, const std::string &fee,
                                       bool enabled)
{
    std::string sql = "INSERT INTO " + db_->table("payment") +
                      " (pay_name, pay_fee, enabled) VALUES (?, ?, ?)";
    db_->execute(sql, {name, shared::Money::normalize(fee), enabled ? "1" : "0"});
}

bool SqlAdminPaymentRepository::patch(int64_t pay_id, const std::optional<std::string> &name,
                                      const std::optional<std::string> &fee,
                                      const std::optional<bool> &enabled)
{
    std::string set_sql;
    Params params;
    auto append = [&set_sql, &params](const std::string &column, const std::string &value) {
        if (!set_sql.empty())
            set_sql += ", ";
        set_sql += column + " = ?";
        params.push_back(value);
    };

    if (name)
        append("pay_name", *name);
    if (fee)
        append("pay_fee", shared::Money::normalize(*fee));
    if (enabled)
        append("enabled", *enabled ? "1" : "0");

    if (set_sql.empty())
        return false;

    return db_->execute("UPDATE " + db_->table("payment") + " SET " + set_sql +
                            " WHERE pay_id = ?",
                        params) > 0;
}

// shipping ----------------------------------------------------------------

std::vector<domain::ShippingOption> SqlAdminShippingRepository::listAll()
{
    std::string sql =
        "SELECT shipping_id AS shipping_id, shipping_name AS shipping_name,"
        " shipping_fee AS shipping_fee FROM " + db_->table("shipping") +
        " ORDER BY shipping_id";
    std::vector<domain::ShippingOption> items;
    for (const Row &row : db_->query(sql, {}))
    {
        domain::ShippingOption item;
        item.shipping_id = row.getInt("shipping_id");
        item.name = row.get("shipping_name");
        item.fee = shared::Money::normalize(row.get("shipping_fee"));
        items.push_back(std::move(item));
    }
    return items;
}

std::optional<domain::ShippingOption> SqlAdminShippingRepository::find(int64_t shipping_id)
{
    std::string sql =
        "SELECT shipping_id AS shipping_id, shipping_name AS shipping_name,"
        " shipping_fee AS shipping_fee FROM " + db_->table("shipping") +
        " WHERE shipping_id = ?";
    std::vector<Row> rows = db_->query(sql, {std::to_string(shipping_id)});
    if (rows.empty())
        return std::nullopt;
    domain::ShippingOption item;
    item.shipping_id = rows.front().getInt("shipping_id");
    item.name = rows.front().get("shipping_name");
    item.fee = shared::Money::normalize(rows.front().get("shipping_fee"));
    return item;
}

void SqlAdminShippingRepository::create(const std::string &name, const std::string &fee,
                                        bool enabled)
{
    std::string sql = "INSERT INTO " + db_->table("shipping") +
                      " (shipping_name, shipping_fee, enabled) VALUES (?, ?, ?)";
    db_->execute(sql, {name, shared::Money::normalize(fee), enabled ? "1" : "0"});
}

bool SqlAdminShippingRepository::patch(int64_t shipping_id,
                                       const std::optional<std::string> &name,
                                       const std::optional<std::string> &fee,
                                       const std::optional<bool> &enabled)
{
    std::string set_sql;
    Params params;
    auto append = [&set_sql, &params](const std::string &column, const std::string &value) {
        if (!set_sql.empty())
            set_sql += ", ";
        set_sql += column + " = ?";
        params.push_back(value);
    };

    if (name)
        append("shipping_name", *name);
    if (fee)
        append("shipping_fee", shared::Money::normalize(*fee));
    if (enabled)
        append("enabled", *enabled ? "1" : "0");

    if (set_sql.empty())
        return false;

    return db_->execute("UPDATE " + db_->table("shipping") + " SET " + set_sql +
                            " WHERE shipping_id = ?",
                        params) > 0;
}

} // namespace ecshop::infra
