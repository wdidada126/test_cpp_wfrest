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

static wfrest::Json orderDetailToJson(const domain::OrderDetail &order)
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
    obj.push_back("consignee", order.consignee);
    obj.push_back("address", order.address);
    obj.push_back("mobile", order.mobile);
    obj.push_back("shipping_id", order.shipping_id);
    obj.push_back("payment_id", order.payment_id);
    obj.push_back("remark", order.remark);

    wfrest::Json::Array items;
    for (const domain::OrderGoods &goods : order.items)
    {
        wfrest::Json::Object item;
        item.push_back("goods_id", goods.goods_id);
        item.push_back("name", goods.name);
        item.push_back("price", goods.price);
        item.push_back("quantity", goods.quantity);
        items.push_back(item);
    }
    obj.push_back("items", items);
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

    // GET /api/v1/me/orders — own order summaries
    sv.GET("/api/v1/me/orders",
           [users, orders](const wfrest::HttpReq *req, wfrest::HttpResp *resp)
    {
        api::ApiResponse err;
        std::optional<int64_t> user_id = api::authenticate(req, users, err);
        if (!user_id)
        {
            api::send(req, resp, err);
            return;
        }

        api::PageQuery page;
        if (!api::parsePage(req, page, err))
        {
            api::send(req, resp, err);
            return;
        }

        domain::OrderPage result = orders->listOfUser(*user_id, page.offset(), page.page_size);

        wfrest::Json::Array items;
        for (const domain::OrderSummary &order : result.items)
            items.push_back(orderToJson(order));

        api::send(req, resp, ApiResponse::ok(api::listBody(page, result.total, items)));
    });

    // GET /api/v1/me/orders/{id} — order detail with goods snapshot
    sv.GET("/api/v1/me/orders/{id}",
           [users, orders](const wfrest::HttpReq *req, wfrest::HttpResp *resp)
    {
        api::ApiResponse err;
        std::optional<int64_t> user_id = api::authenticate(req, users, err);
        if (!user_id)
        {
            api::send(req, resp, err);
            return;
        }

        int64_t order_id = 0;
        if (!api::parsePathId(req, "id", order_id))
        {
            wfrest::Json::Object details;
            details.push_back("field", "id");
            api::send(req, resp, ApiError::validationError("id must be a positive integer", details));
            return;
        }

        std::optional<domain::OrderDetail> order = orders->findOfUser(*user_id, order_id);
        if (!order)
        {
            api::send(req, resp, ApiError::notFound("order not found"));
            return;
        }

        api::send(req, resp, ApiResponse::ok(orderDetailToJson(*order)));
    });

    // POST /api/v1/me/orders/{id}/cancel — cancel a pending_payment order
    sv.POST("/api/v1/me/orders/{id}/cancel",
            [users, orders](const wfrest::HttpReq *req, wfrest::HttpResp *resp)
    {
        api::ApiResponse err;
        std::optional<int64_t> user_id = api::authenticate(req, users, err);
        if (!user_id)
        {
            api::send(req, resp, err);
            return;
        }

        int64_t order_id = 0;
        if (!api::parsePathId(req, "id", order_id))
        {
            wfrest::Json::Object details;
            details.push_back("field", "id");
            api::send(req, resp, ApiError::validationError("id must be a positive integer", details));
            return;
        }

        domain::OrderSummary order;
        domain::OrderCancelStatus status = orders->cancelOfUser(*user_id, order_id, order);

        switch (status)
        {
        case domain::OrderCancelStatus::Cancelled:
            api::send(req, resp, ApiResponse::ok(orderToJson(order)));
            return;
        case domain::OrderCancelStatus::InvalidState:
            api::send(req, resp,
                      ApiError::invalidState("only pending_payment orders can be cancelled"));
            return;
        case domain::OrderCancelStatus::NotFound:
        default:
            api::send(req, resp, ApiError::notFound("order not found"));
            return;
        }
    });

    // POST /api/v1/me/orders/{id}/received — paid -> received
    sv.POST("/api/v1/me/orders/{id}/received",
            [users, orders](const wfrest::HttpReq *req, wfrest::HttpResp *resp)
    {
        api::ApiResponse err;
        std::optional<int64_t> user_id = api::authenticate(req, users, err);
        if (!user_id)
        {
            api::send(req, resp, err);
            return;
        }

        int64_t order_id = 0;
        if (!api::parsePathId(req, "id", order_id))
        {
            wfrest::Json::Object details;
            details.push_back("field", "id");
            api::send(req, resp, ApiError::validationError("id must be a positive integer", details));
            return;
        }

        domain::OrderSummary order;
        domain::OrderCancelStatus status = orders->receivedOfUser(*user_id, order_id, order);

        switch (status)
        {
        case domain::OrderCancelStatus::Cancelled:
            api::send(req, resp, ApiResponse::ok(orderToJson(order)));
            return;
        case domain::OrderCancelStatus::InvalidState:
            api::send(req, resp,
                      ApiError::invalidState("only paid orders can be confirmed as received"));
            return;
        case domain::OrderCancelStatus::NotFound:
        default:
            api::send(req, resp, ApiError::notFound("order not found"));
            return;
        }
    });

    // POST /api/v1/me/orders/{id}/cart — merge order goods back into the cart
    sv.POST("/api/v1/me/orders/{id}/cart",
            [users, orders](const wfrest::HttpReq *req, wfrest::HttpResp *resp)
    {
        api::ApiResponse err;
        std::optional<int64_t> user_id = api::authenticate(req, users, err);
        if (!user_id)
        {
            api::send(req, resp, err);
            return;
        }

        int64_t order_id = 0;
        if (!api::parsePathId(req, "id", order_id))
        {
            wfrest::Json::Object details;
            details.push_back("field", "id");
            api::send(req, resp, ApiError::validationError("id must be a positive integer", details));
            return;
        }

        domain::OrderReturnStatus status = orders->returnToCart(*user_id, order_id);
        switch (status)
        {
        case domain::OrderReturnStatus::Ok:
            api::send(req, resp, ApiResponse::noContent());
            return;
        case domain::OrderReturnStatus::NothingToReturn:
            api::send(req, resp,
                      ApiError::conflict("nothing_to_return",
                                         "no saleable goods with stock to return"));
            return;
        case domain::OrderReturnStatus::NotFound:
        default:
            api::send(req, resp, ApiError::notFound("order not found"));
            return;
        }
    });
}

} // namespace ecshop::http
