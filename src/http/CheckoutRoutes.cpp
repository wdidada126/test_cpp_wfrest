#include "ecshop/http/CheckoutRoutes.h"
#include "ecshop/http/AuthUtil.h"
#include "ecshop/http/HttpUtil.h"
#include "ecshop/infrastructure/SqlCartRepository.h"
#include "ecshop/infrastructure/SqlCheckoutRepository.h"
#include "ecshop/infrastructure/SqlUserRepository.h"

#include <optional>
#include <vector>

namespace ecshop::http {

using api::ApiError;
using api::ApiResponse;

void registerCheckoutRoutes(wfrest::HttpServer &sv, std::shared_ptr<infra::Db> db)
{
    auto users = std::make_shared<infra::SqlUserRepository>(db);
    auto checkout = std::make_shared<infra::SqlCheckoutRepository>(db);
    auto cart = std::make_shared<infra::SqlCartRepository>(db);

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
}

} // namespace ecshop::http
