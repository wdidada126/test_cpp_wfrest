#include "ecshop/http/RegionRoutes.h"
#include "ecshop/http/HttpUtil.h"
#include "ecshop/infrastructure/SqlRegionRepository.h"

#include <cstdlib>
#include <string>

namespace ecshop::http {

using api::ApiError;
using api::ApiResponse;

void registerRegionRoutes(wfrest::HttpServer &sv, std::shared_ptr<infra::Db> db)
{
    auto repo = std::make_shared<infra::SqlRegionRepository>(db);

    // GET /api/v1/regions?parent=&type=&page=&page_size=
    sv.GET("/api/v1/regions", [repo](const wfrest::HttpReq *req, wfrest::HttpResp *resp)
    {
        api::PageQuery page;
        api::ApiResponse err;
        if (!api::parsePage(req, page, err))
        {
            api::send(req, resp, err);
            return;
        }

        int64_t parent_id = 0;
        int64_t region_type = 0;

        for (const std::pair<std::string, int64_t *> param :
             {std::make_pair(std::string("parent"), &parent_id),
              std::make_pair(std::string("type"), &region_type)})
        {
            const std::string &raw = req->query(param.first);
            if (raw.empty())
                continue;

            char *end = nullptr;
            long long v = std::strtoll(raw.c_str(), &end, 10);
            if (!end || *end != '\0' || v < 0)
            {
                wfrest::Json::Object details;
                details.push_back("field", param.first);
                api::send(req, resp,
                          ApiError::validationError(param.first + " must be a non-negative integer",
                                                    details));
                return;
            }
            *param.second = v;
        }

        domain::RegionPage result =
            repo->listRegions(parent_id, region_type, page.offset(), page.page_size);

        wfrest::Json::Array items;
        for (const domain::Region &item : result.items)
        {
            wfrest::Json::Object obj;
            obj.push_back("region_id", item.region_id);
            obj.push_back("parent_id", item.parent_id);
            obj.push_back("name", item.name);
            obj.push_back("region_type", item.region_type);
            items.push_back(obj);
        }

        api::send(req, resp, ApiResponse::ok(api::listBody(page, result.total, items)));
    });
}

} // namespace ecshop::http
