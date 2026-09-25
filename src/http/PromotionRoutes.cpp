#include "ecshop/http/PromotionRoutes.h"
#include "ecshop/http/HttpUtil.h"
#include "ecshop/infrastructure/SqlPromotionRepository.h"
#include "ecshop/shared/TimeUtil.h"

#include <cstdlib>
#include <optional>

namespace ecshop::http {

using api::ApiError;
using api::ApiResponse;

static wfrest::Json promotionToJson(const domain::Promotion &promotion)
{
    wfrest::Json::Object obj;
    obj.push_back("act_id", promotion.act_id);
    obj.push_back("name", promotion.name);
    obj.push_back("description", promotion.description);
    obj.push_back("act_type", promotion.act_type);
    obj.push_back("goods_id", promotion.goods_id);
    obj.push_back("goods_name", promotion.goods_name);
    obj.push_back("start_time", shared::isoUtc(promotion.start_time));
    obj.push_back("end_time", shared::isoUtc(promotion.end_time));
    obj.push_back("ext_info", promotion.ext_info);
    return obj;
}

void registerPromotionRoutes(wfrest::HttpServer &sv, std::shared_ptr<infra::Db> db)
{
    auto promotions = std::make_shared<infra::SqlPromotionRepository>(db);

    // GET /api/v1/promotions?type=0..4 — active promotions
    sv.GET("/api/v1/promotions",
           [promotions](const wfrest::HttpReq *req, wfrest::HttpResp *resp)
    {
        api::PageQuery page;
        api::ApiResponse err;
        if (!api::parsePage(req, page, err))
        {
            api::send(req, resp, err);
            return;
        }

        int64_t act_type = 0;
        const std::string &raw_type = req->query("type");
        if (!raw_type.empty())
        {
            char *end = nullptr;
            long long v = std::strtoll(raw_type.c_str(), &end, 10);
            if (!end || *end != '\0' || v < 0 || v > 4)
            {
                wfrest::Json::Object details;
                details.push_back("field", "type");
                api::send(req, resp,
                          ApiError::validationError("type must be 0-4", details));
                return;
            }
            act_type = v;
        }

        domain::PromotionPage result = promotions->listActive(act_type, page.offset(), page.page_size);

        wfrest::Json::Array items;
        for (const domain::Promotion &promotion : result.items)
            items.push_back(promotionToJson(promotion));

        api::send(req, resp, ApiResponse::ok(api::listBody(page, result.total, items)));
    });

    // GET /api/v1/promotions/{id} — one active promotion
    sv.GET("/api/v1/promotions/{id}", [promotions](const wfrest::HttpReq *req,
                                                    wfrest::HttpResp *resp)
    {
        int64_t act_id = 0;
        if (!api::parsePathId(req, "id", act_id))
        {
            wfrest::Json::Object details;
            details.push_back("field", "id");
            api::send(req, resp, ApiError::validationError("id must be a positive integer", details));
            return;
        }

        std::optional<domain::Promotion> promotion = promotions->findActive(act_id);
        if (!promotion)
        {
            api::send(req, resp, ApiError::notFound("promotion not found"));
            return;
        }

        api::send(req, resp, ApiResponse::ok(promotionToJson(*promotion)));
    });
}

} // namespace ecshop::http
