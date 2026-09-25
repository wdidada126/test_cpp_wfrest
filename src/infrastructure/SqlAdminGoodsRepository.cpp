#include "ecshop/infrastructure/SqlAdminGoodsRepository.h"
#include "ecshop/shared/Money.h"

namespace ecshop::infra {

using domain::AdminGoodsPage;
using domain::AdminGoodsRow;

static const char *kGoodsCols =
    "goods_id AS goods_id, goods_sn AS goods_sn, goods_name AS goods_name,"
    " goods_brief AS goods_brief, shop_price AS shop_price, market_price AS market_price,"
    " goods_number AS stock, cat_id AS cat_id, brand_id AS brand_id,"
    " is_on_sale AS is_on_sale, is_delete AS is_delete,"
    " is_best AS is_best, is_new AS is_new, is_hot AS is_hot, is_promote AS is_promote";

static AdminGoodsRow rowToGoods(const Row &row)
{
    AdminGoodsRow goods;
    goods.goods_id = row.getInt("goods_id");
    goods.goods_sn = row.get("goods_sn");
    goods.name = row.get("goods_name");
    goods.brief = row.get("goods_brief");
    goods.price = shared::Money::normalize(row.get("shop_price"));
    goods.market_price = shared::Money::normalize(row.get("market_price"));
    goods.stock = row.getInt("stock");
    goods.cat_id = row.getInt("cat_id");
    goods.brand_id = row.getInt("brand_id");
    goods.is_on_sale = row.getInt("is_on_sale") != 0;
    goods.is_delete = row.getInt("is_delete") != 0;
    goods.is_best = row.getInt("is_best") != 0;
    goods.is_new = row.getInt("is_new") != 0;
    goods.is_hot = row.getInt("is_hot") != 0;
    goods.is_promote = row.getInt("is_promote") != 0;
    return goods;
}

AdminGoodsPage SqlAdminGoodsRepository::list(const std::string &q, int64_t offset, int64_t limit)
{
    std::string where = "1 = 1";
    Params params;
    if (!q.empty())
    {
        // LIKE keyword escaping so user text cannot inject wildcards
        std::string escaped;
        for (char c : q)
        {
            if (c == '!' || c == '%' || c == '_')
                escaped.push_back('!');
            escaped.push_back(c);
        }
        where = " (g.goods_name LIKE ? ESCAPE '!' OR g.goods_sn LIKE ? ESCAPE '!')";
        params.push_back("%" + escaped + "%");
        params.push_back("%" + escaped + "%");
    }

    AdminGoodsPage page;
    std::string count_sql = "SELECT COUNT(*) AS total FROM " + db_->table("goods") +
                            " g WHERE " + where;
    std::vector<Row> counts = db_->query(count_sql, params);
    if (!counts.empty())
        page.total = counts.front().getInt("total");

    // limit/offset are validated integers formatted by us; user data stays bound.
    std::string sql = "SELECT " + std::string(kGoodsCols) + " FROM " + db_->table("goods") +
                      " g WHERE " + where + " ORDER BY g.goods_id DESC LIMIT " +
                      std::to_string(limit) + " OFFSET " + std::to_string(offset);

    for (const Row &row : db_->query(sql, params))
        page.items.push_back(rowToGoods(row));
    return page;
}

std::optional<AdminGoodsRow> SqlAdminGoodsRepository::find(int64_t goods_id)
{
    std::string sql = "SELECT " + std::string(kGoodsCols) + " FROM " + db_->table("goods") +
                      " g WHERE g.goods_id = ?";
    std::vector<Row> rows = db_->query(sql, {std::to_string(goods_id)});
    if (rows.empty())
        return std::nullopt;
    return rowToGoods(rows.front());
}

int64_t SqlAdminGoodsRepository::create(const AdminGoodsRow &goods,
                                        const std::string &description)
{
    std::string sql =
        "INSERT INTO " + db_->table("goods") +
        " (cat_id, brand_id, goods_sn, goods_name, goods_brief, goods_desc,"
        " shop_price, market_price, goods_number, is_on_sale)"
        " VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)";
    db_->execute(sql, {std::to_string(goods.cat_id), std::to_string(goods.brand_id), goods.name,
                       goods.brief, description, goods.price, goods.market_price,
                       std::to_string(goods.stock), goods.is_on_sale ? "1" : "0"});
    return db_->lastInsertId();
}

bool SqlAdminGoodsRepository::patch(int64_t goods_id, const domain::AdminGoodsPatch &patch)
{
    std::string set_sql;
    Params params;

    auto append = [&set_sql, &params](const std::string &column, const std::string &value) {
        if (!set_sql.empty())
            set_sql += ", ";
        set_sql += column + " = ?";
        params.push_back(value);
    };

    if (patch.name)
        append("goods_name", *patch.name);
    if (patch.brief)
        append("goods_brief", *patch.brief);
    if (patch.description)
        append("goods_desc", *patch.description);
    if (patch.price)
        append("shop_price", shared::Money::normalize(*patch.price));
    if (patch.market_price)
        append("market_price", shared::Money::normalize(*patch.market_price));
    if (patch.stock)
        append("goods_number", std::to_string(*patch.stock));
    if (patch.cat_id)
        append("cat_id", std::to_string(*patch.cat_id));
    if (patch.brand_id)
        append("brand_id", std::to_string(*patch.brand_id));
    if (patch.is_on_sale)
        append("is_on_sale", *patch.is_on_sale ? "1" : "0");
    if (patch.is_best)
        append("is_best", *patch.is_best ? "1" : "0");
    if (patch.is_new)
        append("is_new", *patch.is_new ? "1" : "0");
    if (patch.is_hot)
        append("is_hot", *patch.is_hot ? "1" : "0");
    if (patch.is_promote)
        append("is_promote", *patch.is_promote ? "1" : "0");

    if (set_sql.empty())
        return !find(goods_id) ? false : true;

    return db_->execute("UPDATE " + db_->table("goods") + " SET " + set_sql +
                            " WHERE goods_id = ?",
                        params) > 0;
}

bool SqlAdminGoodsRepository::softDelete(int64_t goods_id)
{
    return db_->execute("UPDATE " + db_->table("goods") +
                            " SET is_delete = 1 WHERE goods_id = ?",
                        {std::to_string(goods_id)}) > 0;
}

} // namespace ecshop::infra
