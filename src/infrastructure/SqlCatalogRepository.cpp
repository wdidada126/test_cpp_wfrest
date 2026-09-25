#include "ecshop/infrastructure/SqlCatalogRepository.h"
#include "ecshop/shared/Money.h"

#include <cstdlib>

namespace ecshop::infra {

using domain::Goods;
using domain::GoodsImage;
using domain::GoodsProduct;
using domain::GoodsSpec;

static std::vector<int64_t> parseAttrIds(const std::string &text)
{
    std::vector<int64_t> ids;
    size_t start = 0;
    while (start < text.size())
    {
        size_t bar = text.find('|', start);
        std::string part = text.substr(start, bar == std::string::npos ? std::string::npos
                                                                     : bar - start);
        if (!part.empty())
            ids.push_back(std::strtoll(part.c_str(), nullptr, 10));
        if (bar == std::string::npos)
            break;
        start = bar + 1;
    }
    return ids;
}

std::optional<Goods> SqlCatalogRepository::findVisibleGoods(int64_t goods_id)
{
    const std::string goods_t = db_->table("goods");
    const std::string category_t = db_->table("category");
    const std::string brand_t = db_->table("brand");

    std::string sql =
        "SELECT g.goods_id AS goods_id, g.cat_id AS cat_id, g.brand_id AS brand_id,"
        " g.goods_sn AS goods_sn, g.goods_name AS goods_name,"
        " g.goods_brief AS goods_brief, g.goods_desc AS goods_desc,"
        " g.shop_price AS shop_price, g.market_price AS market_price,"
        " g.goods_number AS goods_number, c.cat_name AS cat_name,"
        " b.brand_name AS brand_name"
        " FROM " + goods_t + " g"
        " LEFT JOIN " + category_t + " c ON c.cat_id = g.cat_id"
        " LEFT JOIN " + brand_t + " b ON b.brand_id = g.brand_id"
        " WHERE g.goods_id = ? AND g.is_on_sale = 1 AND g.is_delete = 0";

    std::vector<Row> rows = db_->query(sql, {std::to_string(goods_id)});
    if (rows.empty())
        return std::nullopt;

    const Row &row = rows.front();
    Goods g;
    g.goods_id = row.getInt("goods_id");
    g.cat_id = row.getInt("cat_id");
    g.brand_id = row.getInt("brand_id");
    g.goods_sn = row.get("goods_sn");
    g.name = row.get("goods_name");
    g.brief = row.get("goods_brief");
    g.description = row.get("goods_desc");
    g.price = shared::Money::normalize(row.get("shop_price"));
    g.market_price = shared::Money::normalize(row.get("market_price"));
    g.stock_available = row.getInt("goods_number");
    g.is_on_sale = true;
    g.category_name = row.get("cat_name");
    g.brand_name = row.get("brand_name");

    const std::string attr_t = db_->table("goods_attr");
    for (const Row &r : db_->query(
             "SELECT goods_attr_id AS goods_attr_id, attr_id AS attr_id,"
             " attr_value AS attr_value, attr_price AS attr_price FROM " + attr_t +
             " WHERE goods_id = ? ORDER BY goods_attr_id",
             {std::to_string(goods_id)}))
    {
        GoodsSpec spec;
        spec.goods_attr_id = r.getInt("goods_attr_id");
        spec.attr_id = r.getInt("attr_id");
        spec.value = r.get("attr_value");
        spec.price = shared::Money::normalize(r.get("attr_price"));
        g.specs.push_back(std::move(spec));
    }

    const std::string gallery_t = db_->table("goods_gallery");
    for (const Row &r : db_->query(
             "SELECT ga.img_id AS img_id, ga.img_url AS img_url, ga.img_desc AS img_desc, " +
             db_->galleryThumb("ga") + " AS thumb_url, " +
             db_->galleryOriginal("ga") + " AS img_original FROM " + gallery_t +
             " ga WHERE ga.goods_id = ? ORDER BY ga.img_id",
             {std::to_string(goods_id)}))
    {
        GoodsImage img;
        img.img_id = r.getInt("img_id");
        img.url = r.get("img_url");
        img.thumb_url = r.get("thumb_url");
        img.original_url = r.get("img_original");
        img.description = r.get("img_desc");
        g.images.push_back(std::move(img));
    }

    const std::string products_t = db_->table("products");
    for (const Row &r : db_->query(
             "SELECT product_id AS product_id, product_sn AS product_sn,"
             " product_number AS product_number, " + db_->productAttrCol() +
             " AS attr_str FROM " + products_t + " WHERE goods_id = ? ORDER BY product_id",
             {std::to_string(goods_id)}))
    {
        GoodsProduct product;
        product.product_id = r.getInt("product_id");
        product.product_sn = r.get("product_sn");
        product.stock = r.getInt("product_number");
        product.attribute_ids = parseAttrIds(r.get("attr_str"));
        g.products.push_back(std::move(product));
    }

    return g;
}

bool SqlCatalogRepository::categoryVisible(int64_t cat_id)
{
    std::string sql = "SELECT cat_id AS cat_id FROM " + db_->table("category") +
                      " WHERE cat_id = ? AND is_show = 1";
    return !db_->query(sql, {std::to_string(cat_id)}).empty();
}

domain::GoodsPage SqlCatalogRepository::listGoods(const std::string &where_sql,
                                                  const Params &params,
                                                  int64_t offset, int64_t limit)
{
    const std::string goods_t = db_->table("goods");

    domain::GoodsPage page;
    std::string count_sql = "SELECT COUNT(*) AS total FROM " + goods_t + " g WHERE " + where_sql;
    std::vector<Row> counts = db_->query(count_sql, params);
    if (!counts.empty())
        page.total = counts.front().getInt("total");

    // limit/offset are validated integers formatted by us; user data stays bound.
    std::string sql =
        "SELECT g.goods_id AS goods_id, g.goods_sn AS goods_sn,"
        " g.goods_name AS goods_name, g.goods_brief AS goods_brief,"
        " g.shop_price AS shop_price, g.market_price AS market_price"
        " FROM " + goods_t + " g WHERE " + where_sql +
        " ORDER BY g.goods_id LIMIT " + std::to_string(limit) +
        " OFFSET " + std::to_string(offset);

    for (const Row &r : db_->query(sql, params))
    {
        domain::GoodsSummary item;
        item.goods_id = r.getInt("goods_id");
        item.goods_sn = r.get("goods_sn");
        item.name = r.get("goods_name");
        item.brief = r.get("goods_brief");
        item.price = shared::Money::normalize(r.get("shop_price"));
        item.market_price = shared::Money::normalize(r.get("market_price"));
        page.items.push_back(std::move(item));
    }
    return page;
}

domain::GoodsPage SqlCatalogRepository::listCategoryGoods(int64_t cat_id, int64_t offset,
                                                          int64_t limit)
{
    return listGoods("g.cat_id = ? AND g.is_on_sale = 1 AND g.is_delete = 0",
                     {std::to_string(cat_id)}, offset, limit);
}

// LIKE keyword escaping so user text cannot inject wildcards
static std::string likeParam(const std::string &text)
{
    std::string out;
    out.reserve(text.size() + 4);
    for (char c : text)
    {
        if (c == '!' || c == '%' || c == '_')
            out.push_back('!');
        out.push_back(c);
    }
    return "%" + out + "%";
}

domain::GoodsPage SqlCatalogRepository::searchGoods(const domain::GoodsFilter &filter,
                                                    int64_t offset, int64_t limit)
{
    std::string where = "g.is_on_sale = 1 AND g.is_delete = 0";
    Params params;

    if (filter.category_id > 0)
    {
        where += " AND g.cat_id = ?";
        params.push_back(std::to_string(filter.category_id));
    }
    if (filter.brand_id > 0)
    {
        where += " AND g.brand_id = ?";
        params.push_back(std::to_string(filter.brand_id));
    }
    if (!filter.q.empty())
    {
        where += " AND (g.goods_name LIKE ? ESCAPE '!'"
                 " OR g.goods_sn LIKE ? ESCAPE '!'"
                 " OR g.keywords LIKE ? ESCAPE '!')";
        std::string like = likeParam(filter.q);
        params.push_back(like);
        params.push_back(like);
        params.push_back(like);
    }

    const std::string order_goods_t = db_->table("order_goods");
    std::string order_by;
    if (filter.sort == "price_asc")
        order_by = " ORDER BY g.shop_price ASC, g.goods_id ASC";
    else if (filter.sort == "price_desc")
        order_by = " ORDER BY g.shop_price DESC, g.goods_id DESC";
    else if (filter.sort == "newest")
        order_by = " ORDER BY g.add_time DESC, g.goods_id DESC";
    else if (filter.sort == "sales")
        order_by = " ORDER BY (SELECT SUM(og.goods_number) FROM " + order_goods_t +
                   " og WHERE og.goods_id = g.goods_id) DESC, g.goods_id DESC";
    else
        order_by = " ORDER BY g.goods_id ASC";

    domain::GoodsPage page;
    std::string count_sql = "SELECT COUNT(*) AS total FROM " + db_->table("goods") +
                            " g WHERE " + where;
    std::vector<Row> counts = db_->query(count_sql, params);
    if (!counts.empty())
        page.total = counts.front().getInt("total");

    // limit/offset are validated integers formatted by us; user data stays bound.
    std::string sql =
        "SELECT g.goods_id AS goods_id, g.goods_sn AS goods_sn,"
        " g.goods_name AS goods_name, g.goods_brief AS goods_brief,"
        " g.shop_price AS shop_price, g.market_price AS market_price"
        " FROM " + db_->table("goods") + " g WHERE " + where + order_by +
        " LIMIT " + std::to_string(limit) + " OFFSET " + std::to_string(offset);

    for (const Row &r : db_->query(sql, params))
    {
        domain::GoodsSummary item;
        item.goods_id = r.getInt("goods_id");
        item.goods_sn = r.get("goods_sn");
        item.name = r.get("goods_name");
        item.brief = r.get("goods_brief");
        item.price = shared::Money::normalize(r.get("shop_price"));
        item.market_price = shared::Money::normalize(r.get("market_price"));
        page.items.push_back(std::move(item));
    }
    return page;
}

} // namespace ecshop::infra
