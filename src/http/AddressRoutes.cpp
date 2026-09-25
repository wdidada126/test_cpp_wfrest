#include "ecshop/http/AddressRoutes.h"
#include "ecshop/http/AuthUtil.h"
#include "ecshop/http/HttpUtil.h"
#include "ecshop/infrastructure/SqlAddressRepository.h"
#include "ecshop/infrastructure/SqlRegionRepository.h"
#include "ecshop/infrastructure/SqlUserRepository.h"

#include <optional>

namespace ecshop::http {

using api::ApiError;
using api::ApiResponse;

static wfrest::Json addressToJson(const domain::Address &address)
{
    wfrest::Json::Object obj;
    obj.push_back("address_id", address.address_id);
    obj.push_back("consignee", address.consignee);
    obj.push_back("country_id", address.country_id);
    obj.push_back("province_id", address.province_id);
    obj.push_back("city_id", address.city_id);
    obj.push_back("district_id", address.district_id);
    obj.push_back("address", address.address);
    obj.push_back("zip", address.zip);
    obj.push_back("mobile", address.mobile);
    obj.push_back("is_default", address.is_default);
    return obj;
}

void registerAddressRoutes(wfrest::HttpServer &sv, std::shared_ptr<infra::Db> db)
{
    auto users = std::make_shared<infra::SqlUserRepository>(db);
    auto addresses = std::make_shared<infra::SqlAddressRepository>(db);
    auto regions = std::make_shared<infra::SqlRegionRepository>(db);

    // GET /api/v1/me/addresses
    sv.GET("/api/v1/me/addresses",
           [users, addresses](const wfrest::HttpReq *req, wfrest::HttpResp *resp)
    {
        api::ApiResponse err;
        std::optional<int64_t> user_id = api::authenticate(req, users, err);
        if (!user_id)
        {
            api::send(req, resp, err);
            return;
        }

        std::vector<domain::Address> all = addresses->listOfUser(*user_id);

        wfrest::Json::Array items;
        for (const domain::Address &address : all)
            items.push_back(addressToJson(address));

        api::PageQuery page;
        api::send(req, resp,
                  ApiResponse::ok(api::listBody(page, static_cast<int64_t>(all.size()), items)));
    });
}

} // namespace ecshop::http
