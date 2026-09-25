#include "ecshop/infrastructure/SqlGroupBuyRepository.h"
#include "ecshop/shared/Money.h"

namespace ecshop::infra {

static domain::MyGroupBuy rowToGroupBuy(const Row &row)
{
    domain::MyGroupBuy group_buy;
    group_buy.act_id = row.getInt("act_id");
    group_buy.act_name = row.get("act_name");
    group_buy.start_time = row.getInt("start_time");
    group_buy.end_time = row.getInt("end_time");
    group_buy.order_id = row.getInt("order_id");
    group_buy.order_sn = row.get("order_sn");
    group_buy.order_status = row.get("order_status");
    group_buy.order_amount = shared::Money::normalize(row.get("order_amount"));
    return group_buy;
}

std::vector<domain::MyGroupBuy> SqlGroupBuyRepository::listOfUser(int64_t user_id)
{
    // user orders joined with their group buy activity (act_type = 1);
    // finished activities remain in the user's history on purpose
    std::string from = " FROM " + db_->table("order_promotion") + " op JOIN " +
                       db_->table("goods_activity") + " ga ON ga.act_id = op.promotion_id" +
                       " AND ga.act_type = 1 JOIN " + db_->table("order_info") +
                       " oi ON oi.order_id = op.order_id";
    std::string sql =
        "SELECT ga.act_id AS act_id, ga.act_name AS act_name, ga.start_time AS start_time,"
        " ga.end_time AS end_time, oi.order_id AS order_id, oi.order_sn AS order_sn,"
        " oi.order_status AS order_status, oi.order_amount AS order_amount" + from +
        " WHERE op.user_id = ? ORDER BY oi.order_id DESC";

    std::vector<domain::MyGroupBuy> items;
    for (const Row &row : db_->query(sql, {std::to_string(user_id)}))
        items.push_back(rowToGroupBuy(row));
    return items;
}

std::optional<domain::MyGroupBuy> SqlGroupBuyRepository::findOfUser(int64_t user_id,
                                                                    int64_t act_id)
{
    std::string from = " FROM " + db_->table("order_promotion") + " op JOIN " +
                       db_->table("goods_activity") + " ga ON ga.act_id = op.promotion_id" +
                       " AND ga.act_type = 1 JOIN " + db_->table("order_info") +
                       " oi ON oi.order_id = op.order_id";
    std::string sql =
        "SELECT ga.act_id AS act_id, ga.act_name AS act_name, ga.start_time AS start_time,"
        " ga.end_time AS end_time, oi.order_id AS order_id, oi.order_sn AS order_sn,"
        " oi.order_status AS order_status, oi.order_amount AS order_amount" + from +
        " WHERE op.user_id = ? AND ga.act_id = ? ORDER BY oi.order_id DESC LIMIT 1";

    std::vector<Row> rows = db_->query(sql, {std::to_string(user_id), std::to_string(act_id)});
    if (rows.empty())
        return std::nullopt;
    return rowToGroupBuy(rows.front());
}

} // namespace ecshop::infra
