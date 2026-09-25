#include "ecshop/infrastructure/SqlFeedRepository.h"

namespace ecshop::infra {

std::vector<domain::FeedItem> SqlFeedRepository::listActivityItems(int64_t act_type,
                                                                   int64_t limit)
{
    std::string sql =
        "SELECT act_id AS act_id, act_name AS act_name, act_desc AS act_desc,"
        " goods_id AS goods_id FROM " + db_->table("goods_activity") +
        " WHERE act_type = ? AND is_finished = 0 AND start_time <= " + db_->unixNow() +
        " AND end_time >= " + db_->unixNow() +
        " ORDER BY act_id DESC LIMIT " + std::to_string(limit);

    std::vector<domain::FeedItem> items;
    for (const Row &row : db_->query(sql, {std::to_string(act_type)}))
    {
        domain::FeedItem item;
        item.title = row.get("act_name");
        item.link = "/api/v1/goods/" + row.get("goods_id");
        item.description = row.get("act_desc");
        items.push_back(std::move(item));
    }
    return items;
}

std::vector<domain::FeedItem> SqlFeedRepository::listFavourableItems(int64_t limit)
{
    std::string sql =
        "SELECT act_id AS act_id, act_name AS act_name FROM " +
        db_->table("favourable_activity") +
        " WHERE start_time <= " + db_->unixNow() + " AND end_time >= " + db_->unixNow() +
        " ORDER BY act_id DESC LIMIT " + std::to_string(limit);

    std::vector<domain::FeedItem> items;
    for (const Row &row : db_->query(sql, {}))
    {
        domain::FeedItem item;
        item.title = row.get("act_name");
        item.link = "/api/v1/activities";
        item.description = row.get("act_name");
        items.push_back(std::move(item));
    }
    return items;
}

} // namespace ecshop::infra
