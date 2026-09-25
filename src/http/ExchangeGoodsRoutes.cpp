#include "ecshop/http/ExchangeGoodsRoutes.h"
#include "ecshop/http/HttpUtil.h"
#include "ecshop/infrastructure/SqlExchangeGoodsRepository.h"
#include "ecshop/shared/TimeUtil.h"

#include <cstdlib>
#include <optional>
#include <string>

namespace ecshop::http {

using api::ApiError;
using api::ApiResponse;

static wfrest::Json exchangeRowToJson(const domain::ExchangeGoodsRow &item)
{
    wfrest::Json::Object obj;
    obj.push_back("goods_id", item.goods_id);
    obj.push_back("goods_sn", item.goods_sn);
    obj.push_back("name", item.name);
    obj.push_back("category", item.category);
    obj.push_back("price", item.price);
    obj.push_back("exchange_integral", item.integral);
    obj.push_back("is_hot", item.is_hot);
    obj.push_back("last_update",
                  shared::isoUtc(std::strtoll(item.last_update.c_str(), nullptr, 10)));
    return obj;
}

static bool parseExchangeQuery(const wfrest::HttpReq *req,
                               domain::ExchangeGoodsRepository::Query &query, ApiResponse &err)
{
    struct IntField
    {
        const char *name;
        int64_t *target;
    } fields[] = {
        {"category_id", &query.category_id},
        {"integral_min", &query.integral_min},
        {"integral_max", &query.integral_max},
    };
    for (const IntField &field : fields)
    {
        const std::string &raw = req->query(field.name);
        if (raw.empty())
            continue;
        char *end = nullptr;
        long long v = std::strtoll(raw.c_str(), &end, 10);
        if (!end || *end != '\0' || v < 0)
        {
            wfrest::Json::Object details;
            details.push_back("field", std::string(field.name));
            err = ApiError::validationError(std::string(field.name) + " must be an integer",
                                            details);
            return false;
        }
        *field.target = v;
    }

    query.sort = req->query("sort");
    if (!query.sort.empty() && query.sort != "goods_id" &&
        query.sort != "exchange_integral" && query.sort != "last_update")
    {
        wfrest::Json::Object details;
        details.push_back("field", "sort");
        err = ApiError::validationError(
            "sort must be goods_id|exchange_integral|last_update", details);
        return false;
    }

    query.order = req->query("order");
    if (!query.order.empty() && query.order != "asc" && query.order != "desc")
    {
        wfrest::Json::Object details;
        details.push_back("field", "order");
        err = ApiError::validationError("order must be asc|desc", details);
        return false;
    }
    return true;
}

void registerExchangeGoodsRoutes(wfrest::HttpServer &sv, std::shared_ptr<infra::Db> db)
{
    auto exchange = std::make_shared<infra::SqlExchangeGoodsRepository>(db);

    // GET /api/v1/exchange-goods
    sv.GET("/api/v1/exchange-goods",
           [exchange](const wfrest::HttpReq *req, wfrest::HttpResp *resp)
    {
        api::PageQuery page;
        api::ApiResponse err;
        if (!api::parsePage(req, page, err))
        {
            api::send(req, resp, err);
            return;
        }

        domain::ExchangeGoodsRepository::Query query;
        if (!parseExchangeQuery(req, query, err))
        {
            api::send(req, resp, err);
            return;
        }

        domain::ExchangeGoodsPage result = exchange->list(query, page.offset(), page.page_size);

        wfrest::Json::Array items;
        for (const domain::ExchangeGoodsRow &item : result.items)
            items.push_back(exchangeRowToJson(item));

        api::send(req, resp, ApiResponse::ok(api::listBody(page, result.total, items)));
    });

    // GET /api/v1/exchange-goods/{id} — exchange goods detail
    sv.GET("/api/v1/exchange-goods/{id}",
           [exchange](const wfrest::HttpReq *req, wfrest::HttpResp *resp)
    {
        int64_t goods_id = 0;
        if (!api::parsePathId(req, "id", goods_id))
        {
            wfrest::Json::Object details;
            details.push_back("field", "id");
            api::send(req, resp, ApiError::validationError("id must be a positive integer", details));
            return;
        }

        std::optional<domain::ExchangeGoodsRow> item = exchange->findEnabled(goods_id);
        if (!item)
        {
            api::send(req, resp,
                      ApiError::notFound("exchange goods not found or not exchangeable"));
            return;
        }

        api::send(req, resp, ApiResponse::ok(exchangeRowToJson(*item)));
    });
}

} // namespace ecshop::http
