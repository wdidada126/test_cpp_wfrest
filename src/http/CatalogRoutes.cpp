#include "ecshop/http/CatalogRoutes.h"
#include "ecshop/http/HttpUtil.h"
#include "ecshop/infrastructure/SqlCatalogRepository.h"
#include "ecshop/shared/TimeUtil.h"

#include <algorithm>
#include <cstdlib>
#include <optional>
#include <vector>

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

void registerCatalogRoutes(wfrest::HttpServer &sv, std::shared_ptr<infra::Db> db)
{
    auto repo = std::make_shared<infra::SqlCatalogRepository>(db);

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
}

} // namespace ecshop::http
