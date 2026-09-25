#include "ecshop/infrastructure/SqlFavoriteRepository.h"
#include "ecshop/shared/Money.h"

namespace ecshop::infra {

using domain::Favorite;
using domain::FavoriteAddResult;

static Favorite rowToFavorite(const Row &row)
{
    Favorite favorite;
    favorite.rec_id = row.getInt("rec_id");
    favorite.goods_id = row.getInt("goods_id");
    favorite.name = row.get("goods_name");
    favorite.price = shared::Money::normalize(row.get("shop_price"));
    favorite.attention = row.getInt("is_attention") != 0;
    favorite.add_time = row.get("add_time");
    return favorite;
}

std::vector<Favorite> SqlFavoriteRepository::listOfUser(int64_t user_id)
{
    std::string sql =
        "SELECT cg.rec_id AS rec_id, cg.goods_id AS goods_id, g.goods_name AS goods_name,"
        " g.shop_price AS shop_price, cg.is_attention AS is_attention, " +
        db_->toUnix("cg.add_time") + " AS add_time FROM " + db_->table("collect_goods") +
        " cg JOIN " + db_->table("goods") + " g ON g.goods_id = cg.goods_id"
        " WHERE cg.user_id = ? ORDER BY cg.rec_id DESC";

    std::vector<Favorite> items;
    for (const Row &row : db_->query(sql, {std::to_string(user_id)}))
        items.push_back(rowToFavorite(row));
    return items;
}

FavoriteAddResult SqlFavoriteRepository::add(int64_t user_id, int64_t goods_id)
{
    FavoriteAddResult result = FavoriteAddResult::Added;

    db_->transaction([&] {
        std::vector<Row> goods = db_->query(
            "SELECT goods_id AS goods_id FROM " + db_->table("goods") +
                " WHERE goods_id = ? AND is_on_sale = 1 AND is_delete = 0",
            {std::to_string(goods_id)});
        if (goods.empty())
        {
            result = FavoriteAddResult::GoodsNotVisible;
            return;
        }

        std::vector<Row> existing = db_->query(
            "SELECT rec_id AS rec_id FROM " + db_->table("collect_goods") +
                " WHERE user_id = ? AND goods_id = ?",
            {std::to_string(user_id), std::to_string(goods_id)});
        if (!existing.empty())
        {
            result = FavoriteAddResult::Duplicate;
            return;
        }

        try
        {
            db_->execute("INSERT INTO " + db_->table("collect_goods") +
                             " (user_id, goods_id) VALUES (?, ?)",
                         {std::to_string(user_id), std::to_string(goods_id)});
        }
        catch (const DbError &)
        {
            // unique(user_id, goods_id) race
            result = FavoriteAddResult::Duplicate;
        }
    });
    return result;
}

bool SqlFavoriteRepository::setAttention(int64_t user_id, int64_t rec_id, bool attention)
{
    std::string sql = "UPDATE " + db_->table("collect_goods") +
                      " SET is_attention = ? WHERE rec_id = ? AND user_id = ?";
    return db_->execute(sql, {attention ? "1" : "0", std::to_string(rec_id),
                              std::to_string(user_id)}) > 0;
}

bool SqlFavoriteRepository::remove(int64_t user_id, int64_t rec_id)
{
    std::string sql = "DELETE FROM " + db_->table("collect_goods") +
                      " WHERE rec_id = ? AND user_id = ?";
    return db_->execute(sql, {std::to_string(rec_id), std::to_string(user_id)}) > 0;
}

} // namespace ecshop::infra
