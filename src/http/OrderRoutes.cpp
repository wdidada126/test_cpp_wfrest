#include "ecshop/http/OrderRoutes.h"
#include "ecshop/http/AuthUtil.h"
#include "ecshop/http/HttpUtil.h"
#include "ecshop/infrastructure/SqlCheckoutRepository.h"
#include "ecshop/infrastructure/SqlOrderRepository.h"
#include "ecshop/infrastructure/SqlUserRepository.h"
#include "ecshop/shared/Password.h"
#include "ecshop/shared/TimeUtil.h"

#include <cstdlib>
#include <optional>
#include <string>

namespace ecshop::http {

using api::ApiError;
using api::ApiResponse;

static wfrest::Json orderToJson(const domain::OrderSummary &order)
{
    wfrest::Json::Object obj;
    obj.push_back("id", order.order_id);
    obj.push_back("order_sn", order.order_sn);
    obj.push_back("status", order.status);
    obj.push_back("goods_amount", order.goods_amount);
    obj.push_back("shipping_fee", order.shipping_fee);
    obj.push_back("payment_fee", order.payment_fee);
    obj.push_back("order_amount", order.order_amount);
    obj.push_back("replayed", order.replayed);
    return obj;
}

void registerOrderRoutes(wfrest::HttpServer &sv, std::shared_ptr<infra::Db> db)
{
    auto users = std::make_shared<infra::SqlUserRepository>(db);
    auto checkout = std::make_shared<infra::SqlCheckoutRepository>(db);
    auto orders = std::make_shared<infra::SqlOrderRepository>(db);

    // POST /api/v1/orders — requires Idempotency-Key
    sv.POST("/api/v1/orders",
            [users, checkout, orders](const wfrest::HttpReq *req, wfrest::HttpResp *resp)
    {
        api::ApiResponse err;
        std::optional<int64_t> user_id = api::authenticate(req, users, err);
        if (!user_id)
        {
            api::send(req, resp, err);
            return;
        }

        const std::string &idempotency_key = req->header("Idempotency-Key");
        if (idempotency_key.empty() || idempotency_key.size() > 100)
        {
            wfrest::Json::Object details;
            details.push_back("field", "Idempotency-Key");
            api::send(req, resp,
                      ApiError::validationError("Idempotency-Key header is required", details));
            return;
        }

        wfrest::Json body;
        if (!api::parseJsonBody(req, body, err))
        {
            api::send(req, resp, err);
            return;
        }

        domain::PlaceOrderCommand command;
        command.user_id = *user_id;
        command.idempotency_key = idempotency_key;

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
        command.address_id = address_id;
        command.shipping_id = shipping_id;
        command.payment_id = payment_id;

        if (body.has("remark") && !api::readStr(body, "remark", command.remark))
        {
            wfrest::Json::Object details;
            details.push_back("field", "remark");
            api::send(req, resp, ApiError::validationError("remark must be a string", details));
            return;
        }
        if (command.remark.size() > 255)
        {
            wfrest::Json::Object details;
            details.push_back("field", "remark");
            api::send(req, resp, ApiError::validationError("remark too long", details));
            return;
        }

        if (!checkout->addressOwned(command.user_id, command.address_id))
        {
            api::send(req, resp, ApiError::notFound("address not found"));
            return;
        }
        if (!checkout->shippingFee(command.shipping_id))
        {
            wfrest::Json::Object details;
            details.push_back("field", "shipping_id");
            api::send(req, resp,
                      ApiError::validationError("shipping method is not enabled", details));
            return;
        }
        if (!checkout->paymentFee(command.payment_id))
        {
            wfrest::Json::Object details;
            details.push_back("field", "payment_id");
            api::send(req, resp,
                      ApiError::validationError("payment method is not enabled", details));
            return;
        }

        // canonical fingerprint for idempotent replays
        command.fingerprint = shared::sha256Hex(
            std::to_string(command.address_id) + "|" + std::to_string(command.shipping_id) +
            "|" + std::to_string(command.payment_id) + "|" + command.remark);

        domain::OrderSummary order;
        domain::PlaceOrderStatus status = orders->placeOrder(command, order);

        switch (status)
        {
        case domain::PlaceOrderStatus::Created:
            api::send(req, resp, ApiResponse::created(orderToJson(order)));
            return;
        case domain::PlaceOrderStatus::Replayed:
            api::send(req, resp, ApiResponse::ok(orderToJson(order)));
            return;
        case domain::PlaceOrderStatus::KeyMismatch:
            api::send(req, resp,
                      ApiError::conflict("idempotency_key_conflict",
                                         "Idempotency-Key already used with a different request"));
            return;
        case domain::PlaceOrderStatus::OutOfStock:
            api::send(req, resp, ApiError::outOfStock("insufficient stock or unavailable goods"));
            return;
        case domain::PlaceOrderStatus::EmptyCart:
        default:
        {
            wfrest::Json::Object details;
            details.push_back("field", "cart");
            api::send(req, resp,
                      ApiError::validationError("cart has no saleable goods", details));
            return;
        }
        }
    });
}

} // namespace ecshop::http
