#include "ecshop/infrastructure/SqlExchangeGoodsRepository.h"
#include "ecshop/shared/Money.h"

namespace ecshop::infra {

using domain::ExchangeGoodsPage;
using domain::ExchangeGoodsRepository;
using domain::ExchangeGoodsRow;

static const char *kExchangeCols =
    "eg.goods_id AS goods_id, g.goods_sn AS goods_sn, g.goods_name AS goods_name,"
    " c.cat_name AS cat_name, g.shop_price AS shop_price,"
    " eg.exchange_integral AS exchange_integral, eg.is_hot AS is_hot,";

static ExchangeGoodsRow rowToExchange(const Row &row)
{
    ExchangeGoodsRow item;
    item.goods_id = row.getInt("goods_id");
    item.goods_sn = row.get("goods_sn");
    item.name = row.get("goods_name");
    item.category = row.get("cat_name");
    item.price = shared::Money::normalize(row.get("shop_price"));
    item.integral = row.getInt("exchange_integral");
    item.is_hot = row.getInt("is_hot") != 0;
    item.last_update = row.get("last_update");
    return item;
}

static std::string selectExpr(const std::shared_ptr<Db> &db)
{
    // last_update comes from the goods table (exchange_goods has none)
    return std::string(kExchangeCols) + db->toUnix("g.last_update") + " AS last_update";
}

// base FROM W/ WHERE shared by count/list; conds joined with AND
static std::string fromWhere(const std::shared_ptr<Db> &db,
                             const ExchangeGoodsRepository::Query &query, Params &params)
{
    std::string base = " FROM " + db->table("exchange_goods") + " eg JOIN " +
                       db->table("goods") + " g ON g.goods_id = eg.goods_id LEFT JOIN " +
                       db->table("category") + " c ON c.cat_id = g.cat_id";

    std::vector<std::string> conds = {"eg.is_exchange = 1", "g.is_on_sale = 1",
                                      "g.is_delete = 0"};

    if (query.category_id > 0)
    {
        // the category itself plus all descendants; both backends support
        // recursive CTEs
        conds.push_back("g.cat_id IN (WITH RECURSIVE subcats(cat_id) AS ("
                        " SELECT ? UNION SELECT cg.cat_id FROM " + db->table("category") +
                        " cg JOIN subcats s ON cg.parent_id = s.cat_id"
                        " ) SELECT cat_id FROM subcats)");
        params.push_back(std::to_string(query.category_id));
    }

    if (query.integral_min > 0)
    {
        conds.push_back("eg.exchange_integral >= ?");
        params.push_back(std::to_string(query.integral_min));
    }
    if (query.integral_max > 0)
    {
        conds.push_back("eg.exchange_integral <= ?");
        params.push_back(std::to_string(query.integral_max));
    }

    std::string where = conds.front();
    for (size_t i = 1; i < conds.size(); ++i)
        where += " AND " + conds[i];
    return base + " WHERE " + where;
}

ExchangeGoodsPage SqlExchangeGoodsRepository::list(
    const ExchangeGoodsRepository::Query &query, int64_t offset, int64_t limit)
{
    ExchangeGoodsPage page;

    Params params;
    std::string from = fromWhere(db_, query, params);

    std::vector<Row> counts =
        db_->query("SELECT COUNT(*) AS total" + from, params);
    if (!counts.empty())
        page.total = counts.front().getInt("total");

    // sort whitelist; user data stays mir_bound
    std::string sort_col = "eg.goods_id";
    if (query.sort == "exchange_integral")
        sort_col = "eg.exchange_integral";
    else if (query.sort == "last_update")
        sort_col = "g.last_update";
    std::string order = query.order == "desc" ? " DESC" : " ASC";

    // limit/offset are validated integers formatted by us.
    std::string sql =
        "SELECT " + selectExpr(db_) + from +
        " ORDER BY " + sort_col + order + ", eg.goods_id ASC"
        " LIMIT " + std::to_string(limit) + " OFFSET " + std::to_string(offset);

    for (const Row &row : db_->query(sql, params))
        page.items.push_back(rowToExchange(row));
    return page;
}

std::optional<ExchangeGoodsRow> SqlExchangeGoodsRepository::findEnabled(int64_t goods_id)
{
    std::string sql =
        "SELECT " + selectExpr(db_) + " FROM " + db_->table("exchange_goods") +
        " eg JOIN " + db_->table("goods") + " g ON g.goods_id = eg.goods_id LEFT JOIN " +
        db_->table("category") + " c ON c.cat_id = g.cat_id"
        " WHERE eg.goods_id = ? AND eg.is_exchange = 1 AND g.is_on_sale = 1 AND g.is_delete = 0";

    std::vector<Row> rows = db_->query(sql, {std::to_string(goods_id)});
    if (rows.empty())
        return std::nullopt;
    return rowToExchange(rows.front());
}

} // namespace ecshop::infra
