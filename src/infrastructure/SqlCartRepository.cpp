#include "ecshop/infrastructure/SqlCartRepository.h"
#include "ecshop/shared/Money.h"

namespace ecshop::infra {

using domain::CartItem;
using domain::CartAddResult;

static CartItem rowToCartItem(const Row &row)
{
    CartItem item;
    item.id = row.getInt("rec_id");
    item.goods_id = row.getInt("goods_id");
    item.name = row.get("goods_name");
    item.price = shared::Money::normalize(row.get("shop_price"));
    item.quantity = row.getInt("goods_number");
    item.version = row.getInt("version");
    return item;
}

std::vector<CartItem> SqlCartRepository::listOfUser(int64_t user_id)
{
    std::string sql =
        "SELECT c.rec_id AS rec_id, c.goods_id AS goods_id, g.goods_name AS goods_name,"
        " g.shop_price AS shop_price, c.goods_number AS goods_number, c.version AS version"
        " FROM " + db_->table("cart") + " c JOIN " + db_->table("goods") +
        " g ON g.goods_id = c.goods_id WHERE c.user_id = ? ORDER BY c.rec_id DESC";

    std::vector<CartItem> items;
    for (const Row &row : db_->query(sql, {std::to_string(user_id)}))
        items.push_back(rowToCartItem(row));
    return items;
}

std::optional<CartItem> SqlCartRepository::findByGoods(int64_t user_id, int64_t goods_id)
{
    std::string sql =
        "SELECT c.rec_id AS rec_id, c.goods_id AS goods_id, g.goods_name AS goods_name,"
        " g.shop_price AS shop_price, c.goods_number AS goods_number, c.version AS version"
        " FROM " + db_->table("cart") + " c JOIN " + db_->table("goods") +
        " g ON g.goods_id = c.goods_id WHERE c.user_id = ? AND c.goods_id = ?";
    std::vector<Row> rows = db_->query(sql, {std::to_string(user_id), std::to_string(goods_id)});
    if (rows.empty())
        return std::nullopt;
    return rowToCartItem(rows.front());
}

CartAddResult SqlCartRepository::add(int64_t user_id, int64_t goods_id, int64_t quantity,
                                     CartItem &out)
{
    CartAddResult result = CartAddResult::Ok;

    db_->transaction([&] {
        std::vector<Row> goods = db_->query(
            "SELECT goods_id AS goods_id FROM " + db_->table("goods") +
                " WHERE goods_id = ? AND is_on_sale = 1 AND is_delete = 0",
            {std::to_string(goods_id)});
        if (goods.empty())
        {
            result = CartAddResult::GoodsNotVisible;
            return;
        }

        std::string cart_t = db_->table("cart");
        // accumulate atomically; portable upsert (update first, insert on miss)
        int64_t changed = db_->execute(
            "UPDATE " + cart_t +
                " SET goods_number = goods_number + ?, version = version + 1"
                " WHERE user_id = ? AND goods_id = ?",
            {std::to_string(quantity), std::to_string(user_id), std::to_string(goods_id)});
        if (changed == 0)
        {
            try
            {
                db_->execute("INSERT INTO " + cart_t +
                                 " (user_id, goods_id, goods_number, version) VALUES (?, ?, ?, 1)",
                             {std::to_string(user_id), std::to_string(goods_id),
                              std::to_string(quantity)});
            }
            catch (const DbError &)
            {
                // unique(user_id, goods_id) race: fall back to accumulating
                db_->execute("UPDATE " + cart_t +
                                 " SET goods_number = goods_number + ?, version = version + 1"
                                 " WHERE user_id = ? AND goods_id = ?",
                             {std::to_string(quantity), std::to_string(user_id),
                              std::to_string(goods_id)});
            }
        }

        std::optional<CartItem> item = findByGoods(user_id, goods_id);
        if (item)
            out = *item;
    });
    return result;
}

std::optional<bool> SqlCartRepository::updateQuantity(int64_t user_id, int64_t rec_id,
                                                      int64_t quantity, int64_t version)
{
    std::string cart_t = db_->table("cart");

    std::optional<bool> ok;
    db_->transaction([&] {
        std::vector<Row> rows = db_->query(
            "SELECT version AS version FROM " + cart_t + " WHERE rec_id = ? AND user_id = ?",
            {std::to_string(rec_id), std::to_string(user_id)});
        if (rows.empty())
            return;

        if (rows.front().getInt("version") != version)
        {
            ok = false; // stale version
            return;
        }

        db_->execute("UPDATE " + cart_t +
                         " SET goods_number = ?, version = version + 1"
                         " WHERE rec_id = ? AND user_id = ? AND version = ?",
                     {std::to_string(quantity), std::to_string(rec_id),
                      std::to_string(user_id), std::to_string(version)});
        ok = true;
    });
    return ok;
}

bool SqlCartRepository::remove(int64_t user_id, int64_t rec_id)
{
    std::string sql = "DELETE FROM " + db_->table("cart") +
                      " WHERE rec_id = ? AND user_id = ?";
    return db_->execute(sql, {std::to_string(rec_id), std::to_string(user_id)}) > 0;
}

} // namespace ecshop::infra
