#include "ecshop/infrastructure/SqlTagRepository.h"

namespace ecshop::infra {

using domain::TagCount;

std::vector<TagCount> SqlTagRepository::statsOfGoods(int64_t goods_id)
{
    // limit-free aggregate over one goods
    std::string sql =
        "SELECT tag_words AS word, COUNT(*) AS count FROM " + db_->table("tag") +
        " WHERE goods_id = ? GROUP BY tag_words ORDER BY count DESC, tag_words ASC";

    std::vector<TagCount> items;
    for (const Row &row : db_->query(sql, {std::to_string(goods_id)}))
    {
        TagCount item;
        item.word = row.get("word");
        item.count = row.getInt("count");
        items.push_back(std::move(item));
    }
    return items;
}

bool SqlTagRepository::addTags(int64_t user_id, int64_t goods_id,
                               const std::vector<std::string> &words,
                               std::vector<TagCount> &stats)
{
    std::string tag_t = db_->table("tag");
    std::string goods_t = db_->table("goods");

    bool ok = true;
    db_->transaction([&] {
        // single INSERT with a saleable-goods EXISTS condition; the unique
        // constraint drops same user+goods+word repeats
        std::string sql =
            "INSERT INTO " + tag_t + " (user_id, goods_id, tag_words)"
            " SELECT ?, ?, ? WHERE EXISTS (SELECT 1 FROM " + goods_t +
            " WHERE goods_id = ? AND is_on_sale = 1 AND is_delete = 0)";

        for (const std::string &word : words)
        {
            try
            {
                db_->execute(sql, {std::to_string(user_id), std::to_string(goods_id), word,
                                   std::to_string(goods_id)});
            }
            catch (const DbError &)
            {
                // unique(user, goods, word) or FK failure; visible-check misses
                // surface as zero inserted rows below
            }
        }

        std::vector<Row> visible = db_->query(
            "SELECT goods_id AS goods_id FROM " + goods_t +
                " WHERE goods_id = ? AND is_on_sale = 1 AND is_delete = 0",
            {std::to_string(goods_id)});
        ok = !visible.empty();
        if (ok)
            stats = statsOfGoods(goods_id);
    });
    return ok;
}

std::vector<TagCount> SqlTagRepository::listOfUser(int64_t user_id)
{
    std::string sql =
        "SELECT tag_words AS word, COUNT(*) AS count FROM " + db_->table("tag") +
        " WHERE user_id = ? GROUP BY tag_words ORDER BY count DESC, tag_words ASC";

    std::vector<TagCount> items;
    for (const Row &row : db_->query(sql, {std::to_string(user_id)}))
    {
        TagCount item;
        item.word = row.get("word");
        item.count = row.getInt("count");
        items.push_back(std::move(item));
    }
    return items;
}

void SqlTagRepository::removeTag(int64_t user_id, const std::string &word)
{
    std::string sql = "DELETE FROM " + db_->table("tag") +
                      " WHERE user_id = ? AND tag_words = ?";
    db_->execute(sql, {std::to_string(user_id), word});
}

} // namespace ecshop::infra
