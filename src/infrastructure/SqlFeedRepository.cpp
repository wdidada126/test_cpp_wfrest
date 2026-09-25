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

std::vector<domain::AdImage> SqlFeedRepository::listImageAds()
{
    std::string sql =
        "SELECT ad_id AS ad_id, position_id AS position_id, ad_name AS ad_name,"
        " ad_link AS ad_link, ad_code AS ad_code FROM " + db_->table("ad") +
        " WHERE media_type = 0 AND enabled = 1 AND start_time <= " + db_->nowExpr() +
        " AND end_time >= " + db_->nowExpr() +
        " ORDER BY position_id, ad_id";

    std::vector<domain::AdImage> items;
    for (const Row &row : db_->query(sql, {}))
    {
        domain::AdImage item;
        item.ad_id = row.getInt("ad_id");
        item.position_id = row.getInt("position_id");
        item.name = row.get("ad_name");
        item.link = row.get("ad_link");
        item.code = row.get("ad_code");
        items.push_back(std::move(item));
    }
    return items;
}

std::optional<domain::Ad> SqlFeedRepository::findActive(int64_t ad_id)
{
    std::string sql =
        "SELECT ad_id AS ad_id, position_id AS position_id, media_type AS media_type,"
        " ad_name AS ad_name, ad_link AS ad_link, ad_code AS ad_code,"
        " click_count AS click_count FROM " + db_->table("ad") +
        " WHERE ad_id = ? AND enabled = 1 AND start_time <= " + db_->nowExpr() +
        " AND end_time >= " + db_->nowExpr();

    std::vector<Row> rows = db_->query(sql, {std::to_string(ad_id)});
    if (rows.empty())
        return std::nullopt;

    domain::Ad ad;
    ad.ad_id = rows.front().getInt("ad_id");
    ad.position_id = rows.front().getInt("position_id");
    ad.media_type = rows.front().getInt("media_type");
    ad.name = rows.front().get("ad_name");
    ad.link = rows.front().get("ad_link");
    ad.code = rows.front().get("ad_code");
    ad.click_count = rows.front().getInt("click_count");
    return ad;
}

domain::AdClickResult SqlFeedRepository::recordClick(int64_t ad_id, const std::string &referer)
{
    domain::AdClickResult result;

    db_->transaction([&] {
        std::optional<domain::Ad> ad = findActive(ad_id);
        if (!ad)
            return;

        db_->execute("UPDATE " + db_->table("ad") +
                         " SET click_count = click_count + 1 WHERE ad_id = ?",
                     {std::to_string(ad_id)});

        // adsense upsert: update the referer counter, insert when new
        int64_t changed = db_->execute(
            "UPDATE " + db_->table("adsense") + " SET clicks = clicks + 1" +
                " WHERE from_ad = ? AND referer = ?",
            {std::to_string(ad_id), referer});
        if (changed == 0)
        {
            try
            {
                db_->execute("INSERT INTO " + db_->table("adsense") +
                                 " (from_ad, referer, clicks) VALUES (?, ?, 1)",
                             {std::to_string(ad_id), referer});
            }
            catch (const DbError &)
            {
                db_->execute("UPDATE " + db_->table("adsense") +
                                 " SET clicks = clicks + 1 WHERE from_ad = ? AND referer = ?",
                             {std::to_string(ad_id), referer});
            }
        }

        result.status = domain::AdClickStatus::Clicked;
        result.redirect_url = ad->link;
    });
    return result;
}

} // namespace ecshop::infra
