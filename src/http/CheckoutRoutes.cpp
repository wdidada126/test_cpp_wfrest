#include "ecshop/http/CheckoutRoutes.h"
#include "ecshop/http/AuthUtil.h"
#include "ecshop/http/HttpUtil.h"
#include "ecshop/infrastructure/SqlCartRepository.h"
#include "ecshop/infrastructure/SqlCheckoutRepository.h"
#include "ecshop/infrastructure/SqlRegionRepository.h"
#include "ecshop/infrastructure/SqlUserRepository.h"
#include "ecshop/shared/Money.h"

#include <cstdlib>
#include <optional>
#include <string>
#include <vector>

namespace ecshop::http {

using api::ApiError;
using api::ApiResponse;

static wfrest::Json checkoutItemToJson(const domain::CartItem &item)
{
    wfrest::Json::Object obj;
    obj.push_back("id", item.id);
    obj.push_back("goods_id", item.goods_id);
    obj.push_back("name", item.name);
    obj.push_back("price", item.price);
    obj.push_back("quantity", item.quantity);
    obj.push_back("version", item.version);
    return obj;
}

void registerCheckoutRoutes(wfrest::HttpServer &sv, std::shared_ptr<infra::Db> db)
{
    auto users = std::make_shared<infra::SqlUserRepository>(db);
    auto checkout = std::make_shared<infra::SqlCheckoutRepository>(db);
    auto cart = std::make_shared<infra::SqlCartRepository>(db);
    auto regions = std::make_shared<infra::SqlRegionRepository>(db);

    // GET /api/v1/checkout/options — enabled shipping and payment methods
    sv.GET("/api/v1/checkout/options",
           [users, checkout](const wfrest::HttpReq *req, wfrest::HttpResp *resp)
    {
        api::ApiResponse err;
        if (!api::authenticate(req, users, err))
        {
            api::send(req, resp, err);
            return;
        }

        wfrest::Json::Array shipping;
        for (const domain::ShippingOption &option : checkout->listShipping())
        {
            wfrest::Json::Object obj;
            obj.push_back("shipping_id", option.shipping_id);
            obj.push_back("name", option.name);
            obj.push_back("fee", option.fee);
            shipping.push_back(obj);
        }

        wfrest::Json::Array payment;
        for (const domain::PaymentOption &option : checkout->listPayment())
        {
            wfrest::Json::Object obj;
            obj.push_back("payment_id", option.payment_id);
            obj.push_back("name", option.name);
            obj.push_back("fee", option.fee);
            payment.push_back(obj);
        }

        wfrest::Json::Object out;
        out.push_back("shipping", shipping);
        out.push_back("payment", payment);
        api::send(req, resp, ApiResponse::ok(out));
    });

    // POST /api/v1/checkout/quote — server-side total preview
    sv.POST("/api/v1/checkout/quote",
            [users, checkout, cart](const wfrest::HttpReq *req, wfrest::HttpResp *resp)
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

        int64_t address_id = 0, shipping_id = 0, payment_id = 0;
        struct IdField
        {
            const char *name;
            int64_t *target;
        } fields[] = {
            {"address_id", &address_id},
            {"shipping_id", &shipping_id},
            {"payment_id", &payment_id},
        };
        for (const IdField &field : fields)
        {
            if (!api::readInt(body, field.name, *field.target) || *field.target <= 0)
            {
                wfrest::Json::Object details;
                details.push_back("field", std::string(field.name));
                api::send(req, resp,
                          ApiError::validationError(std::string(field.name) + " is required",
                                                    details));
                return;
            }
        }

        if (!checkout->addressOwned(*user_id, address_id))
        {
            api::send(req, resp, ApiError::notFound("address not found"));
            return;
        }

        std::optional<std::string> shipping_fee = checkout->shippingFee(shipping_id);
        std::optional<std::string> payment_fee = checkout->paymentFee(payment_id);
        if (!shipping_fee)
        {
            wfrest::Json::Object details;
            details.push_back("field", "shipping_id");
            api::send(req, resp,
                      ApiError::validationError("shipping method is not enabled", details));
            return;
        }
        if (!payment_fee)
        {
            wfrest::Json::Object details;
            details.push_back("field", "payment_id");
            api::send(req, resp,
                      ApiError::validationError("payment method is not enabled", details));
            return;
        }

        std::vector<domain::CartItem> items = cart->listOfUser(*user_id);
        if (items.empty())
        {
            wfrest::Json::Object details;
            details.push_back("field", "cart");
            api::send(req, resp, ApiError::validationError("cart has no saleable goods", details));
            return;
        }

        int64_t goods_cents = 0;
        wfrest::Json::Array json_items;
        for (const domain::CartItem &item : items)
        {
            int64_t unit_cents = 0;
            shared::Money::parse(item.price, unit_cents);
            goods_cents += unit_cents * item.quantity;
            json_items.push_back(checkoutItemToJson(item));
        }

        int64_t shipping_cents = 0, payment_cents = 0;
        shared::Money::parse(*shipping_fee, shipping_cents);
        shared::Money::parse(*payment_fee, payment_cents);

        wfrest::Json::Object out;
        out.push_back("address_id", address_id);
        out.push_back("shipping_id", shipping_id);
        out.push_back("payment_id", payment_id);
        out.push_back("items", json_items);
        out.push_back("goods_amount", shared::Money::format(goods_cents));
        out.push_back("shipping_fee", shared::Money::format(shipping_cents));
        out.push_back("payment_fee", shared::Money::format(payment_cents));
        out.push_back("order_amount",
                      shared::Money::format(goods_cents + shipping_cents + payment_cents));
        api::send(req, resp, ApiResponse::ok(out));
    });

    // GET /api/v1/shipping-options?country_id=&province_id=&city_id=&district_id=
    // public myship.php equivalent; fee table is global for now (docs/03)
    sv.GET("/api/v1/shipping-options",
           [regions, checkout](const wfrest::HttpReq *req, wfrest::HttpResp *resp)
    {
        int64_t country_id = 0, province_id = 0, city_id = 0, district_id = 0;
        struct IdField
        {
            const char *name;
            int64_t *target;
        } fields[] = {
            {"country_id", &country_id},
            {"province_id", &province_id},
            {"city_id", &city_id},
            {"district_id", &district_id},
        };
        api::ApiResponse err;
        for (const IdField &field : fields)
        {
            const std::string &raw = req->query(field.name);
            if (raw.empty())
                continue;
            char *end = nullptr;
            long long v = std::strtoll(raw.c_str(), &end, 10);
            if (!end || *end != '\0' || v <= 0)
            {
                wfrest::Json::Object details;
                details.push_back("field", std::string(field.name));
                api::send(req, resp,
                          ApiError::validationError(std::string(field.name) +
                                                    " must be a positive integer", details));
                return;
            }
            *field.target = v;
        }

        // per-level parent legality checks (only for provided levels)
        if (province_id > 0)
        {
            if (country_id <= 0)
            {
                api::send(req, resp,
                          ApiError::validationError("province_id requires country_id"));
                return;
            }
            std::optional<domain::Region> province = regions->find(province_id);
            if (!province || province->parent_id != country_id)
            {
                api::send(req, resp,
                          ApiError::validationError("province does not belong to country"));
                return;
            }
        }
        if (city_id > 0)
        {
            if (province_id <= 0)
            {
                api::send(req, resp, ApiError::validationError("city_id requires province_id"));
                return;
            }
            std::optional<domain::Region> city = regions->find(city_id);
            if (!city || city->parent_id != province_id)
            {
                api::send(req, resp,
                          ApiError::validationError("city does not belong to province"));
                return;
            }
        }
        if (district_id > 0)
        {
            if (city_id <= 0)
            {
                api::send(req, resp,
                          ApiError::validationError("district_id requires city_id"));
                return;
            }
            std::optional<domain::Region> district = regions->find(district_id);
            if (!district || district->parent_id != city_id)
            {
                api::send(req, resp,
                          ApiError::validationError("district does not belong to city"));
                return;
            }
        }

        // pivot rows
        auto regionArray = [](const std::vector<domain::Region> &items) {
            wfrest::Json::Array arr;
            for (const domain::Region &region : items)
            {
                wfrest::Json::Object it;
                it.push_back("region_id", region.region_id);
                it.push_back("name", region.name);
                it.push_back("region_type", region.region_type);
                arr.push_back(it);
            }
            return wfrest::Json(arr);
        };

        wfrest::Json::Object cascade;
        cascade.push_back("countries", regionArray(regions->listChildren(0)));

        if (country_id > 0)
            cascade.push_back("provinces", regionArray(regions->listChildren(country_id)));
        if (province_id > 0)
            cascade.push_back("cities", regionArray(regions->listChildren(province_id)));
        if (city_id > 0)
            cascade.push_back("districts", regionArray(regions->listChildren(city_id)));

        wfrest::Json::Object selected;
        selected.push_back("country_id", country_id);
        selected.push_back("province_id", province_id);
        selected.push_back("city_id", city_id);
        selected.push_back("district_id", district_id);

        wfrest::Json::Array shipping;
        for (const domain::ShippingOption &option : checkout->listShipping())
        {
            wfrest::Json::Object it;
            it.push_back("shipping_id", option.shipping_id);
            it.push_back("name", option.name);
            it.push_back("fee", option.fee);
            shipping.push_back(it);
        }

        wfrest::Json::Object out;
        out.push_back("selected", selected);
        out.push_back("regions", cascade);
        out.push_back("shipping", shipping);
        api::send(req, resp, ApiResponse::ok(out));
    });
}

} // namespace ecshop::http
