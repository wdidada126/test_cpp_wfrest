#include "ecshop/http/AddressRoutes.h"
#include "ecshop/http/AuthUtil.h"
#include "ecshop/http/HttpUtil.h"
#include "ecshop/infrastructure/SqlAddressRepository.h"
#include "ecshop/infrastructure/SqlRegionRepository.h"
#include "ecshop/infrastructure/SqlUserRepository.h"

#include <optional>
#include <string>

namespace ecshop::http {

using api::ApiError;
using api::ApiResponse;

// Parses and validates the shared address JSON body. Returns false with err set.
static bool parseAddressBody(const wfrest::Json &body,
                             const std::shared_ptr<infra::SqlRegionRepository> &regions,
                             domain::Address &out, ApiResponse &err)
{
    if (!api::readStr(body, "consignee", out.consignee) || out.consignee.empty() ||
        out.consignee.size() > 60)
    {
        wfrest::Json::Object details;
        details.push_back("field", "consignee");
        err = ApiError::validationError("consignee must be 1-60 bytes", details);
        return false;
    }

    struct RegionLevel
    {
        const char *field;
        int64_t *target;
        int64_t type;
    } levels[] = {
        {"country_id", &out.country_id, 1},
        {"province_id", &out.province_id, 2},
        {"city_id", &out.city_id, 3},
        {"district_id", &out.district_id, 4},
    };

    for (const RegionLevel &level : levels)
    {
        if (!api::readInt(body, level.field, *level.target) || *level.target <= 0)
        {
            wfrest::Json::Object details;
            details.push_back("field", std::string(level.field));
            err = ApiError::validationError(std::string(level.field) + " is required", details);
            return false;
        }
    }

    // legal hierarchy: each region exists at its level and parents chain up
    std::optional<domain::Region> country = regions->find(out.country_id);
    std::optional<domain::Region> province = regions->find(out.province_id);
    std::optional<domain::Region> city = regions->find(out.city_id);
    std::optional<domain::Region> district = regions->find(out.district_id);

    bool legal = country && province && city && district &&
                 country->region_type == 1 && province->region_type == 2 &&
                 city->region_type == 3 && district->region_type == 4 &&
                 province->parent_id == country->region_id &&
                 city->parent_id == province->region_id &&
                 district->parent_id == city->region_id;
    if (!legal)
    {
        wfrest::Json::Object details;
        details.push_back("field", "district_id");
        err = ApiError::validationError("region ids do not form a legal hierarchy", details);
        return false;
    }

    if (!api::readStr(body, "address", out.address) || out.address.empty() ||
        out.address.size() > 255)
    {
        wfrest::Json::Object details;
        details.push_back("field", "address");
        err = ApiError::validationError("address must be 1-255 bytes", details);
        return false;
    }

    out.zip.clear();
    if (body.has("zip") && (!api::readStr(body, "zip", out.zip) || out.zip.size() > 20))
    {
        wfrest::Json::Object details;
        details.push_back("field", "zip");
        err = ApiError::validationError("zip must be at most 20 bytes", details);
        return false;
    }

    out.mobile.clear();
    if (body.has("mobile") &&
        (!api::readStr(body, "mobile", out.mobile) || out.mobile.size() > 32))
    {
        wfrest::Json::Object details;
        details.push_back("field", "mobile");
        err = ApiError::validationError("mobile must be at most 32 bytes", details);
        return false;
    }

    out.is_default = false;
    if (body.has("is_default") && !api::readBool(body, "is_default", out.is_default))
    {
        wfrest::Json::Object details;
        details.push_back("field", "is_default");
        err = ApiError::validationError("is_default must be a boolean", details);
        return false;
    }

    return true;
}

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

    // POST /api/v1/me/addresses
    sv.POST("/api/v1/me/addresses",
            [users, addresses, regions](const wfrest::HttpReq *req, wfrest::HttpResp *resp)
    {
        api::ApiResponse err;
        std::optional<int64_t> user_id = api::authenticate(req, users, err);
        if (!user_id)
        {
            api::send(req, resp, err);
            return;
        }

        wfrest::Json body;
        if (!api::parseJsonBody(req, body, err))
        {
            api::send(req, resp, err);
            return;
        }

        domain::Address address;
        if (!parseAddressBody(body, regions, address, err))
        {
            api::send(req, resp, err);
            return;
        }

        address.address_id = addresses->create(*user_id, address);
        api::send(req, resp, ApiResponse::created(addressToJson(address)));
    });
}

} // namespace ecshop::http
