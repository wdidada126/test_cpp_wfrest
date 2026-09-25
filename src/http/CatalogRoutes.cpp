#include "ecshop/http/CatalogRoutes.h"
#include "ecshop/http/HttpUtil.h"
#include "ecshop/infrastructure/SqlArticleRepository.h"
#include "ecshop/infrastructure/SqlCatalogRepository.h"
#include "ecshop/shared/TimeUtil.h"

#include <algorithm>
#include <cstdlib>
#include <optional>
#include <string>
#include <vector>

#include "ecshop/shared/Money.h"

namespace ecshop::http {

using api::ApiError;
using api::ApiResponse;

static wfrest::Json goodsToJson(const domain::Goods &g)
{
    wfrest::Json::Object obj;
    obj.push_back("goods_id", g.goods_id);
    obj.push_back("goods_sn", g.goods_sn);
    obj.push_back("name", g.name);
    obj.push_back("brief", g.brief);
    obj.push_back("description", g.description);
    obj.push_back("price", g.price);
    obj.push_back("market_price", g.market_price);
    obj.push_back("currency", "CNY");
    obj.push_back("stock_available", g.stock_available);

    wfrest::Json::Object category;
    category.push_back("cat_id", g.cat_id);
    category.push_back("name", g.category_name);
    obj.push_back("category", category);

    wfrest::Json::Object brand;
    brand.push_back("brand_id", g.brand_id);
    brand.push_back("name", g.brand_name);
    obj.push_back("brand", brand);

    wfrest::Json::Array images;
    for (const domain::GoodsImage &img : g.images)
    {
        wfrest::Json::Object it;
        it.push_back("img_id", img.img_id);
        it.push_back("url", img.url);
        it.push_back("thumb_url", img.thumb_url);
        it.push_back("original_url", img.original_url);
        it.push_back("description", img.description);
        images.push_back(it);
    }
    obj.push_back("images", images);

    wfrest::Json::Array specs;
    for (const domain::GoodsSpec &spec : g.specs)
    {
        wfrest::Json::Object it;
        it.push_back("goods_attr_id", spec.goods_attr_id);
        it.push_back("attr_id", spec.attr_id);
        it.push_back("value", spec.value);
        it.push_back("price", spec.price);
        specs.push_back(it);
    }
    obj.push_back("specs", specs);

    wfrest::Json::Array products;
    for (const domain::GoodsProduct &p : g.products)
    {
        wfrest::Json::Object it;
        it.push_back("product_id", p.product_id);
        it.push_back("product_sn", p.product_sn);
        it.push_back("stock", p.stock);

        wfrest::Json::Array attrs;
        for (int64_t id : p.attribute_ids)
            attrs.push_back(id);
        it.push_back("attribute_ids", attrs);
        products.push_back(it);
    }
    obj.push_back("products", products);

    return obj;
}

static wfrest::Json goodsSummaryToJson(const domain::GoodsSummary &item)
{
    wfrest::Json::Object obj;
    obj.push_back("goods_id", item.goods_id);
    obj.push_back("goods_sn", item.goods_sn);
    obj.push_back("name", item.name);
    obj.push_back("brief", item.brief);
    obj.push_back("price", item.price);
    obj.push_back("market_price", item.market_price);
    return obj;
}

static wfrest::Json categoryToJson(const domain::CategorySummary &item)
{
    wfrest::Json::Object obj;
    obj.push_back("cat_id", item.cat_id);
    obj.push_back("parent_id", item.parent_id);
    obj.push_back("name", item.name);
    obj.push_back("goods_count", item.goods_count);
    return obj;
}

static wfrest::Json articleSummaryToJson(const domain::ArticleSummary &item)
{
    wfrest::Json::Object obj;
    obj.push_back("article_id", item.article_id);
    obj.push_back("title", item.title);
    obj.push_back("author", item.author);
    obj.push_back("description", item.description);
    return obj;
}

static wfrest::Json brandToJson(const domain::BrandSummary &item)
{
    wfrest::Json::Object obj;
    obj.push_back("brand_id", item.brand_id);
    obj.push_back("name", item.name);
    obj.push_back("logo", item.logo);
    obj.push_back("site_url", item.site_url);
    obj.push_back("goods_count", item.goods_count);
    return obj;
}

void registerCatalogRoutes(wfrest::HttpServer &sv, std::shared_ptr<infra::Db> db)
{
    auto repo = std::make_shared<infra::SqlCatalogRepository>(db);
    auto articles = std::make_shared<infra::SqlArticleRepository>(db);

    // GET /api/v1/goods/{id} — goods detail (docs/08_first_url_tutorial.md)
    sv.GET("/api/v1/goods/{id}", [repo](const wfrest::HttpReq *req, wfrest::HttpResp *resp)
    {
        int64_t goods_id = 0;
        if (!api::parsePathId(req, "id", goods_id))
        {
            wfrest::Json::Object details;
            details.push_back("field", "id");
            api::send(req, resp, ApiError::validationError("id must be a positive integer", details));
            return;
        }

        std::optional<domain::Goods> goods = repo->findVisibleGoods(goods_id);
        if (!goods)
        {
            api::send(req, resp, ApiError::notFound("goods not found"));
            return;
        }

        api::send(req, resp, ApiResponse::ok(goodsToJson(*goods)));
    });

    // GET /api/v1/categories/{id}/goods?page=&page_size=
    sv.GET("/api/v1/categories/{id}/goods",
           [repo](const wfrest::HttpReq *req, wfrest::HttpResp *resp)
    {
        int64_t cat_id = 0;
        if (!api::parsePathId(req, "id", cat_id))
        {
            wfrest::Json::Object details;
            details.push_back("field", "id");
            api::send(req, resp, ApiError::validationError("id must be a positive integer", details));
            return;
        }

        api::PageQuery page;
        api::ApiResponse err;
        if (!api::parsePage(req, page, err))
        {
            api::send(req, resp, err);
            return;
        }

        if (!repo->categoryVisible(cat_id))
        {
            api::send(req, resp, ApiError::notFound("category not found"));
            return;
        }

        domain::GoodsPage result = repo->listCategoryGoods(cat_id, page.offset(), page.page_size);

        wfrest::Json::Array items;
        for (const domain::GoodsSummary &item : result.items)
            items.push_back(goodsSummaryToJson(item));

        api::send(req, resp, ApiResponse::ok(api::listBody(page, result.total, items)));
    });

    // GET /api/v1/goods?q=&category_id=&brand_id=&sort=&page=&page_size=
    sv.GET("/api/v1/goods", [repo](const wfrest::HttpReq *req, wfrest::HttpResp *resp)
    {
        api::PageQuery page;
        api::ApiResponse err;
        if (!api::parsePage(req, page, err))
        {
            api::send(req, resp, err);
            return;
        }

        domain::GoodsFilter filter;
        filter.q = req->query("q");

        for (const char *key : {"category_id", "brand_id"})
        {
            const std::string &raw = req->query(key);
            if (raw.empty())
                continue;

            char *end = nullptr;
            long long v = std::strtoll(raw.c_str(), &end, 10);
            if (!end || *end != '\0' || v < 0)
            {
                wfrest::Json::Object details;
                details.push_back("field", std::string(key));
                api::send(req, resp,
                          ApiError::validationError(std::string(key) + " must be an integer",
                                                    details));
                return;
            }
            if (std::string(key) == "category_id")
                filter.category_id = v;
            else
                filter.brand_id = v;
        }

        filter.sort = req->query("sort");
        static const std::vector<std::string> kSorts = {
            "", "price_asc", "price_desc", "newest", "sales"};
        if (std::find(kSorts.begin(), kSorts.end(), filter.sort) == kSorts.end())
        {
            wfrest::Json::Object details;
            details.push_back("field", "sort");
            api::send(req, resp,
                      ApiError::validationError(
                          "sort must be one of price_asc|price_desc|newest|sales", details));
            return;
        }

        domain::GoodsPage result = repo->searchGoods(filter, page.offset(), page.page_size);

        wfrest::Json::Array items;
        for (const domain::GoodsSummary &item : result.items)
            items.push_back(goodsSummaryToJson(item));

        api::send(req, resp, ApiResponse::ok(api::listBody(page, result.total, items)));
    });

    // POST /api/v1/goods/{id}/price-quote — display-only quote (docs/03)
    sv.POST("/api/v1/goods/{id}/price-quote",
            [repo](const wfrest::HttpReq *req, wfrest::HttpResp *resp)
    {
        int64_t goods_id = 0;
        if (!api::parsePathId(req, "id", goods_id))
        {
            wfrest::Json::Object details;
            details.push_back("field", "id");
            api::send(req, resp, ApiError::validationError("id must be a positive integer", details));
            return;
        }

        wfrest::Json body;
        api::ApiResponse err;
        if (!api::parseJsonBody(req, body, err))
        {
            api::send(req, resp, err);
            return;
        }

        int64_t quantity = 0;
        if (!api::readInt(body, "quantity", quantity))
        {
            wfrest::Json::Object details;
            details.push_back("field", "quantity");
            api::send(req, resp, ApiError::validationError("quantity is required", details));
            return;
        }
        if (quantity < 1 || quantity > 999)
        {
            wfrest::Json::Object details;
            details.push_back("field", "quantity");
            api::send(req, resp,
                      ApiError::validationError("quantity must be between 1 and 999", details));
            return;
        }

        int64_t product_id = 0;
        if (body.has("product_id") && !api::readInt(body, "product_id", product_id))
        {
            wfrest::Json::Object details;
            details.push_back("field", "product_id");
            api::send(req, resp,
                      ApiError::validationError("product_id must be a positive integer", details));
            return;
        }

        std::vector<int64_t> attribute_ids;
        if (body.has("attribute_ids"))
        {
            wfrest::Json arr = body["attribute_ids"];
            if (!arr.is_array())
            {
                wfrest::Json::Object details;
                details.push_back("field", "attribute_ids");
                api::send(req, resp,
                          ApiError::validationError("attribute_ids must be an array", details));
                return;
            }
            for (size_t i = 0; i < arr.size(); ++i)
            {
                wfrest::Json v = arr[static_cast<int>(i)];
                if (!v.is_number())
                {
                    wfrest::Json::Object details;
                    details.push_back("field", "attribute_ids");
                    api::send(req, resp,
                              ApiError::validationError("attribute_ids must be integers", details));
                    return;
                }
                int64_t attr_id = static_cast<int64_t>(v.get<double>());
                if (attr_id <= 0)
                {
                    wfrest::Json::Object details;
                    details.push_back("field", "attribute_ids");
                    api::send(req, resp,
                              ApiError::validationError("attribute_ids must be positive integers",
                                                        details));
                    return;
                }
                attribute_ids.push_back(attr_id);
            }
        }

        std::optional<domain::Goods> goods = repo->findVisibleGoods(goods_id);
        if (!goods)
        {
            api::send(req, resp, ApiError::notFound("goods not found"));
            return;
        }

        if (product_id > 0)
        {
            bool found = false;
            for (const domain::GoodsProduct &p : goods->products)
            {
                if (p.product_id == product_id)
                {
                    found = true;
                    break;
                }
            }
            if (!found)
            {
                wfrest::Json::Object details;
                details.push_back("field", "product_id");
                api::send(req, resp,
                          ApiError::validationError("product_id does not belong to this goods",
                                                    details));
                return;
            }
        }

        for (int64_t attr_id : attribute_ids)
        {
            bool found = false;
            for (const domain::GoodsSpec &spec : goods->specs)
            {
                if (spec.goods_attr_id == attr_id)
                {
                    found = true;
                    break;
                }
            }
            if (!found)
            {
                wfrest::Json::Object details;
                details.push_back("field", "attribute_ids");
                api::send(req, resp,
                          ApiError::validationError("attribute_ids contains unknown attribute",
                                                    details));
                return;
            }
        }

        // Display-only quote: unit price is the goods shop price; the order
        // pipeline recomputes prices, attribute add-ons included.
        int64_t unit_cents = 0;
        shared::Money::parse(goods->price, unit_cents);
        int64_t total_cents = unit_cents * quantity;

        wfrest::Json::Object out;
        out.push_back("goods_id", goods->goods_id);
        out.push_back("quantity", quantity);
        out.push_back("unit_price", shared::Money::format(unit_cents));
        out.push_back("total", shared::Money::format(total_cents));
        out.push_back("currency", "CNY");
        out.push_back("stock_available", goods->stock_available);
        api::send(req, resp, ApiResponse::ok(out));
    });

    // GET /api/v1/home — home page aggregates (goods, categories, articles)
    sv.GET("/api/v1/home", [repo, articles](const wfrest::HttpReq *req, wfrest::HttpResp *resp)
    {
        wfrest::Json::Object out;

        wfrest::Json::Array goods_arr;
        for (const domain::GoodsSummary &item : repo->listNewestGoods(10))
            goods_arr.push_back(goodsSummaryToJson(item));
        out.push_back("goods", goods_arr);

        wfrest::Json::Array cat_arr;
        for (const domain::CategorySummary &item : repo->listVisibleCategories())
            cat_arr.push_back(categoryToJson(item));
        out.push_back("categories", cat_arr);

        wfrest::Json::Array article_arr;
        for (const domain::ArticleSummary &item : articles->listLatestOpen(5))
            article_arr.push_back(articleSummaryToJson(item));
        out.push_back("articles", article_arr);

        api::send(req, resp, ApiResponse::ok(out));
    });

    // GET /api/v1/brands?page=&page_size=
    sv.GET("/api/v1/brands", [repo](const wfrest::HttpReq *req, wfrest::HttpResp *resp)
    {
        api::PageQuery page;
        api::ApiResponse err;
        if (!api::parsePage(req, page, err))
        {
            api::send(req, resp, err);
            return;
        }

        domain::BrandPage result = repo->listVisibleBrands(page.offset(), page.page_size);

        wfrest::Json::Array items;
        for (const domain::BrandSummary &item : result.items)
            items.push_back(brandToJson(item));

        api::send(req, resp, ApiResponse::ok(api::listBody(page, result.total, items)));
    });

    // GET /api/v1/brands/{id}/goods?page=&page_size=
    sv.GET("/api/v1/brands/{id}/goods",
           [repo](const wfrest::HttpReq *req, wfrest::HttpResp *resp)
    {
        int64_t brand_id = 0;
        if (!api::parsePathId(req, "id", brand_id))
        {
            wfrest::Json::Object details;
            details.push_back("field", "id");
            api::send(req, resp, ApiError::validationError("id must be a positive integer", details));
            return;
        }

        api::PageQuery page;
        api::ApiResponse err;
        if (!api::parsePage(req, page, err))
        {
            api::send(req, resp, err);
            return;
        }

        if (!repo->brandVisible(brand_id))
        {
            api::send(req, resp, ApiError::notFound("brand not found"));
            return;
        }

        domain::GoodsPage result = repo->listBrandGoods(brand_id, page.offset(), page.page_size);

        wfrest::Json::Array items;
        for (const domain::GoodsSummary &item : result.items)
            items.push_back(goodsSummaryToJson(item));

        api::send(req, resp, ApiResponse::ok(api::listBody(page, result.total, items)));
    });
}

} // namespace ecshop::http
