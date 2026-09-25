#include "ecshop/infrastructure/SqlPromotionRepository.h"
#include "ecshop/shared/Money.h"

namespace ecshop::infra {

using domain::Promotion;
using domain::PromotionPage;

static const char *kPromotionCols =
    "act_id AS act_id, act_name AS act_name, act_desc AS act_desc, act_type AS act_type,"
    " goods_id AS goods_id, goods_name AS goods_name, start_time AS start_time,"
    " end_time AS end_time, ext_info AS ext_info";

static Promotion rowToPromotion(const Row &row)
{
    Promotion promotion;
    promotion.act_id = row.getInt("act_id");
    promotion.name = row.get("act_name");
    promotion.description = row.get("act_desc");
    promotion.act_type = row.getInt("act_type");
    promotion.goods_id = row.getInt("goods_id");
    promotion.goods_name = row.get("goods_name");
    promotion.start_time = row.getInt("start_time");
    promotion.end_time = row.getInt("end_time");
    promotion.ext_info = row.get("ext_info");
    return promotion;
}

static std::string activeWhere(const std::shared_ptr<Db> &db)
{
    // time window + still visible goods
    return std::string("ga.is_finished = 0 AND ga.start_time <= ") + db->unixNow() +
           " AND ga.end_time >= " + db->unixNow() +
           " AND EXISTS (SELECT 1 FROM " + db->table("goods") +
           " g WHERE g.goods_id = ga.goods_id AND g.is_on_sale = 1 AND g.is_delete = 0)";
}

PromotionPage SqlPromotionRepository::listActive(int64_t act_type, int64_t offset, int64_t limit)
{
    PromotionPage page;

    std::string where = activeWhere(db_);
    if (act_type > 0)
        where += " AND ga.act_type = " + std::to_string(act_type);

    std::vector<Row> counts = db_->query(
        "SELECT COUNT(*) AS total FROM " + db_->table("goods_activity") + " ga WHERE " + where, {});
    if (!counts.empty())
        page.total = counts.front().getInt("total");

    // limit/offset are validated integers formatted by us; no user data here.
    std::string sql =
        "SELECT " + std::string(kPromotionCols) + " FROM " + db_->table("goods_activity") +
        " ga WHERE " + where + " ORDER BY ga.act_id DESC LIMIT " + std::to_string(limit) +
        " OFFSET " + std::to_string(offset);

    for (const Row &row : db_->query(sql, {}))
        page.items.push_back(rowToPromotion(row));
    return page;
}

std::optional<Promotion> SqlPromotionRepository::findActive(int64_t act_id)
{
    std::string sql = "SELECT " + std::string(kPromotionCols) + " FROM " +
                      db_->table("goods_activity") + " ga WHERE ga.act_id = ? AND " +
                      activeWhere(db_);

    std::vector<Row> rows = db_->query(sql, {std::to_string(act_id)});
    if (rows.empty())
        return std::nullopt;
    return rowToPromotion(rows.front());
}

std::vector<domain::Favourable> SqlPromotionRepository::listFavourableActive(int64_t offset,
                                                                            int64_t limit)
{
    std::string sql =
        "SELECT act_id AS act_id, act_name AS act_name, start_time AS start_time,"
        " end_time AS end_time, user_rank AS user_rank, act_range AS act_range,"
        " act_range_ext AS act_range_ext, min_amount AS min_amount, max_amount AS max_amount,"
        " act_type AS act_type, act_type_ext AS act_type_ext, gift AS gift FROM " +
        db_->table("favourable_activity") +
        " WHERE start_time <= " + db_->unixNow() + " AND end_time >= " + db_->unixNow() +
        " ORDER BY sort_order, act_id LIMIT " + std::to_string(limit) +
        " OFFSET " + std::to_string(offset);

    std::vector<domain::Favourable> items;
    for (const Row &row : db_->query(sql, {}))
    {
        domain::Favourable item;
        item.act_id = row.getInt("act_id");
        item.name = row.get("act_name");
        item.start_time = row.getInt("start_time");
        item.end_time = row.getInt("end_time");
        item.user_rank = row.get("user_rank");
        item.act_range = row.getInt("act_range");
        item.act_range_ext = row.get("act_range_ext");
        item.min_amount = shared::Money::normalize(row.get("min_amount"));
        item.max_amount = shared::Money::normalize(row.get("max_amount"));
        item.act_type = row.getInt("act_type");
        item.act_type_ext = row.get("act_type_ext");
        item.gift = row.get("gift");
        items.push_back(std::move(item));
    }
    return items;
}

} // namespace ecshop::infra
