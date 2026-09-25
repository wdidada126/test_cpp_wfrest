#include "ecshop/infrastructure/SqlAffiliateRepository.h"
#include "ecshop/shared/Money.h"

namespace ecshop::infra {

std::vector<domain::AffiliateLevel> SqlAffiliateRepository::levelsOf(int64_t user_id)
{
    // recursive CTE over user_referral; at most 10 levels of referrals
    std::string referral_t = db_->table("user_referral");
    std::string sql =
        "WITH RECURSIVE down(user_id, level) AS ("
        " SELECT user_id, 0 FROM " + referral_t + " WHERE referrer_user_id = ?"
        " UNION ALL"
        " SELECT ur.user_id, d.level + 1 FROM " + referral_t +
        " ur JOIN down d ON ur.referrer_user_id = d.user_id WHERE d.level < 10"
        ") SELECT level AS level, COUNT(*) AS users FROM down GROUP BY level ORDER BY level";

    std::vector<domain::AffiliateLevel> items;
    for (const Row &row : db_->query(sql, {std::to_string(user_id)}))
    {
        domain::AffiliateLevel item;
        item.level = row.getInt("level");
        item.users = row.getInt("users");
        items.push_back(std::move(item));
    }
    return items;
}

int64_t SqlAffiliateRepository::ordersCount(int64_t orders_user)
{
    std::string referral_t = db_->table("user_referral");
    std::string info_t = db_->table("order_info");

    std::string sql =
        "SELECT COUNT(*) AS total FROM " + info_t + " oi JOIN " + referral_t +
        " ur ON ur.user_id = oi.user_id WHERE ur.referrer_user_id = ?";
    std::vector<Row> rows = db_->query(sql, {std::to_string(orders_user)});
    return rows.empty() ? 0 : rows.front().getInt("total");
}

std::vector<domain::AffiliateOrderRow> SqlAffiliateRepository::ordersOf(int64_t user_id,
                                                                       int64_t offset,
                                                                       int64_t limit)
{
    std::string referral_t = db_->table("user_referral");
    std::string info_t = db_->table("order_info");
    std::string log_t = db_->table("affiliate_log");

    std::string sql =
        "SELECT oi.order_id AS order_id, oi.order_sn AS order_sn,"
        " oi.order_amount AS order_amount, lg.money AS money, lg.point AS point,"
        " lg.separate_type AS separate_type," +
        db_->toUnix("oi." + db_->orderTimeCol()) + " AS created_at FROM " + info_t + " oi JOIN " +
        referral_t + " ur ON ur.user_id = oi.user_id"
                     " LEFT JOIN " +
        log_t + " lg ON lg.order_id = oi.order_id AND lg.user_id = ?"
                " WHERE ur.referrer_user_id = ?"
                " ORDER BY oi.order_id DESC LIMIT " +
        std::to_string(limit) + " OFFSET " + std::to_string(offset);

    std::vector<domain::AffiliateOrderRow> items;
    Params params = {std::to_string(user_id), std::to_string(user_id)};
    for (const Row &row : db_->query(sql, params))
    {
        domain::AffiliateOrderRow item;
        item.order_id = row.getInt("order_id");
        item.order_amount = shared::Money::normalize(row.get("order_amount"));

        // legacy rule: mask the order number
        std::string sn = row.get("order_sn");
        if (sn.size() > 4)
            item.order_sn_masked = sn.substr(0, 2) + "****" + sn.substr(sn.size() - 2);
        else
            item.order_sn_masked = "****";

        item.separated = row.has("money") && !row.get("money").empty();
        item.money = shared::Money::normalize(row.get("money").empty()
                                                  ? "0.00"
                                                  : row.get("money"));
        item.point = row.getInt("point");
        item.separate_type = row.getInt("separate_type");
        item.created_at = row.get("created_at");
        items.push_back(std::move(item));
    }
    return items;
}

} // namespace ecshop::infra
