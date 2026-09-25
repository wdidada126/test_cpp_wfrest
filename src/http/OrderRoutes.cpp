#include "ecshop/http/OrderRoutes.h"
#include "ecshop/http/AuthUtil.h"
#include "ecshop/http/HttpUtil.h"
#include "ecshop/infrastructure/SqlAffiliateRepository.h"
#include "ecshop/infrastructure/SqlCheckoutRepository.h"
#include "ecshop/infrastructure/SqlGroupBuyRepository.h"
#include "ecshop/infrastructure/SqlOrderRepository.h"
#include "ecshop/infrastructure/SqlUserRepository.h"
#include "ecshop/shared/Money.h"
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

    // GET /api/v1/me/orders/by-number/{order_sn}/status — status by order number
    sv.GET("/api/v1/me/orders/by-number/{order_sn}/status",
           [users, orders](const wfrest::HttpReq *req, wfrest::HttpResp *resp)
    {
        api::ApiResponse err;
        std::optional<int64_t> user_id = api::authenticate(req, users, err);
        if (!user_id)
        {
            api::send(req, resp, err);
            return;
        }

        const std::string &order_sn = req->param("order_sn");
        if (order_sn.empty() || order_sn.size() > 40)
        {
            wfrest::Json::Object details;
            details.push_back("field", "order_sn");
            api::send(req, resp, ApiError::validationError("order_sn is required", details));
            return;
        }

        std::optional<domain::OrderSummary> order = orders->findBySnOfUser(*user_id, order_sn);
        if (!order)
        {
            api::send(req, resp, ApiError::notFound("order not found"));
            return;
        }

        api::send(req, resp, ApiResponse::ok(orderToJson(*order)));
    });

    // PATCH /api/v1/me/orders/{id}/address — pending_payment + unshipped only
    sv.PATCH("/api/v1/me/orders/{id}/address",
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

        wfrest::Json body;
        if (!api::parseJsonBody(req, body, err))
        {
            api::send(req, resp, err);
            return;
        }

        domain::DeliveryAddressPatch patch;
        struct StrField
        {
            const char *name;
            std::string *target;
            bool required;
        } fields[] = {
            {"consignee", &patch.consignee, true},
            {"email", &patch.email, true},
            {"address", &patch.address, true},
            {"zipcode", &patch.zipcode, false},
            {"tel", &patch.tel, false},
            {"mobile", &patch.mobile, false},
            {"sign_building", &patch.sign_building, false},
            {"best_time", &patch.best_time, false},
        };
        for (const StrField &field : fields)
        {
            if (!api::readStr(body, field.name, *field.target))
            {
                if (!field.required)
                    continue; // optional, kept empty
                wfrest::Json::Object details;
                details.push_back("field", std::string(field.name));
                api::send(req, resp,
                          ApiError::validationError(std::string(field.name) + " is required",
                                                    details));
                return;
            }
        }
        size_t at = patch.email.find('@');
        bool email_ok = at != std::string::npos && at > 0 && at + 1 < patch.email.size();
        if (!email_ok)
        {
            wfrest::Json::Object details;
            details.push_back("field", "email");
            api::send(req, resp,
                      ApiError::validationError("email must be a valid address", details));
            return;
        }

        domain::OrderPatchStatus status = orders->updateAddressOfUser(*user_id, order_id, patch);
        switch (status)
        {
        case domain::OrderPatchStatus::Ok:
        {
            wfrest::Json::Object out;
            out.push_back("id", order_id);
            out.push_back("consignee", patch.consignee);
            api::send(req, resp, ApiResponse::ok(out));
            return;
        }
        case domain::OrderPatchStatus::InvalidState:
            api::send(req, resp,
                      ApiError::invalidState("order is no longer pending_payment/unshipped"));
            return;
        case domain::OrderPatchStatus::NotFound:
        default:
            api::send(req, resp, ApiError::notFound("order not found"));
            return;
        }
    });

    // PATCH /api/v1/me/orders/{id}/payment — {"payment_id":2}
    sv.PATCH("/api/v1/me/orders/{id}/payment",
             [users, orders, checkout](const wfrest::HttpReq *req, wfrest::HttpResp *resp)
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

        wfrest::Json body;
        if (!api::parseJsonBody(req, body, err))
        {
            api::send(req, resp, err);
            return;
        }

        int64_t payment_id = 0;
        if (!api::readInt(body, "payment_id", payment_id) || payment_id <= 0)
        {
            wfrest::Json::Object details;
            details.push_back("field", "payment_id");
            api::send(req, resp,
                      ApiError::validationError("payment_id must be a positive integer", details));
            return;
        }

        domain::OrderPatchStatus status = orders->updatePaymentOfUser(*user_id, order_id, payment_id);
        switch (status)
        {
        case domain::OrderPatchStatus::Ok:
        {
            wfrest::Json::Object out;
            out.push_back("id", order_id);
            out.push_back("payment_id", payment_id);
            api::send(req, resp, ApiResponse::ok(out));
            return;
        }
        case domain::OrderPatchStatus::InvalidState:
            api::send(req, resp, ApiError::conflict("payment_unchanged",
                                                    "payment method unchanged or state changed"));
            return;
        case domain::OrderPatchStatus::NotFound:
        default:
            api::send(req, resp, ApiError::notFound("order or payment method not found"));
            return;
        }
    });

    // PATCH /api/v1/me/orders/{id}/surplus — {"amount":"10.00"}
    sv.PATCH("/api/v1/me/orders/{id}/surplus",
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

        wfrest::Json body;
        if (!api::parseJsonBody(req, body, err))
        {
            api::send(req, resp, err);
            return;
        }

        std::string amount_text;
        int64_t amount_cents = 0;
        if (!api::readStr(body, "amount", amount_text) ||
            !shared::Money::parse(amount_text, amount_cents) || amount_cents <= 0)
        {
            wfrest::Json::Object details;
            details.push_back("field", "amount");
            api::send(req, resp,
                      ApiError::validationError("amount must be a positive decimal string",
                                                details));
            return;
        }

        domain::SurplusResult result = orders->payWithSurplus(*user_id, order_id, amount_cents);
        switch (result.status)
        {
        case domain::OrderSurplusStatus::PartiallyPaid:
        case domain::OrderSurplusStatus::FullyPaid:
        {
            wfrest::Json::Object out;
            out.push_back("id", order_id);
            out.push_back("applied", result.applied);
            out.push_back("paid_total", result.paid_total);
            out.push_back("remaining", result.remaining);
            out.push_back("status", result.became_paid ? "paid" : "pending_payment");
            api::send(req, resp, ApiResponse::ok(out));
            return;
        }
        case domain::OrderSurplusStatus::InvalidState:
            api::send(req, resp,
                      ApiError::conflict("surplus_failed",
                                         "insufficient balance or order not payable"));
            return;
        case domain::OrderSurplusStatus::NotFound:
        default:
            api::send(req, resp, ApiError::notFound("order not found"));
            return;
        }
    });

    // POST /api/v1/me/orders/merge — {"from_order_id":101,"to_order_id":102}
    sv.POST("/api/v1/me/orders/merge",
            [users, orders](const wfrest::HttpReq *req, wfrest::HttpResp *resp)
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

        int64_t from_order_id = 0, to_order_id = 0;
        if (!api::readInt(body, "from_order_id", from_order_id) || from_order_id <= 0 ||
            !api::readInt(body, "to_order_id", to_order_id) || to_order_id <= 0)
        {
            wfrest::Json::Object details;
            details.push_back("field", "from_order_id");
            api::send(req, resp,
                      ApiError::validationError("from_order_id and to_order_id are required",
                                                details));
            return;
        }

        domain::OrderSummary merged;
        domain::OrderMergeStatus status =
            orders->mergeOrders(*user_id, from_order_id, to_order_id, merged);
        switch (status)
        {
        case domain::OrderMergeStatus::Merged:
            api::send(req, resp, ApiResponse::created(orderToJson(merged)));
            return;
        case domain::OrderMergeStatus::InvalidState:
            api::send(req, resp,
                      ApiError::conflict("merge_conflict",
                                         "orders must be distinct, own, pending_payment and "
                                         "without balance usage"));
            return;
        case domain::OrderMergeStatus::NotFound:
        default:
            api::send(req, resp, ApiError::notFound("order not found"));
            return;
        }
    });

    auto group_buys = std::make_shared<infra::SqlGroupBuyRepository>(db);

    // GET /api/v1/me/group-buys — own group buy order history
    sv.GET("/api/v1/me/group-buys",
           [users, group_buys](const wfrest::HttpReq *req, wfrest::HttpResp *resp)
    {
        api::ApiResponse err;
        std::optional<int64_t> user_id = api::authenticate(req, users, err);
        if (!user_id)
        {
            api::send(req, resp, err);
            return;
        }

        std::vector<domain::MyGroupBuy> all = group_buys->listOfUser(*user_id);

        auto to_json = [](const domain::MyGroupBuy &item) {
            wfrest::Json::Object obj;
            obj.push_back("act_id", item.act_id);
            obj.push_back("act_name", item.act_name);
            obj.push_back("start_time", shared::isoUtc(item.start_time));
            obj.push_back("end_time", shared::isoUtc(item.end_time));
            obj.push_back("order_id", item.order_id);
            obj.push_back("order_sn", item.order_sn);
            obj.push_back("order_status", item.order_status);
            obj.push_back("order_amount", item.order_amount);
            return obj;
        };

        wfrest::Json::Array items;
        api::PageQuery page;
        for (const domain::MyGroupBuy &item : all)
            items.push_back(to_json(item));

        api::send(req, resp,
                  ApiResponse::ok(api::listBody(page, static_cast<int64_t>(all.size()), items)));
    });

    // GET /api/v1/me/group-buys/{id} — one group buy activity with order snapshot
    sv.GET("/api/v1/me/group-buys/{id}",
           [users, group_buys](const wfrest::HttpReq *req, wfrest::HttpResp *resp)
    {
        api::ApiResponse err;
        std::optional<int64_t> user_id = api::authenticate(req, users, err);
        if (!user_id)
        {
            api::send(req, resp, err);
            return;
        }

        int64_t act_id = 0;
        if (!api::parsePathId(req, "id", act_id))
        {
            wfrest::Json::Object details;
            details.push_back("field", "id");
            api::send(req, resp, ApiError::validationError("id must be a positive integer", details));
            return;
        }

        std::optional<domain::MyGroupBuy> item = group_buys->findOfUser(*user_id, act_id);
        if (!item)
        {
            api::send(req, resp, ApiError::notFound("group buy not found"));
            return;
        }

        wfrest::Json::Object out;
        out.push_back("act_id", item->act_id);
        out.push_back("act_name", item->act_name);
        out.push_back("start_time", shared::isoUtc(item->start_time));
        out.push_back("end_time", shared::isoUtc(item->end_time));
        out.push_back("order_id", item->order_id);
        out.push_back("order_sn", item->order_sn);
        out.push_back("order_status", item->order_status);
        out.push_back("order_amount", item->order_amount);
        api::send(req, resp, ApiResponse::ok(out));
    });

    auto affiliate = std::make_shared<infra::SqlAffiliateRepository>(db);
    sv.GET("/api/v1/me/affiliate",
           [users, affiliate](const wfrest::HttpReq *req, wfrest::HttpResp *resp)
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

        wfrest::Json::Object out;

        wfrest::Json::Array levels;
        for (const domain::AffiliateLevel &level : affiliate->levelsOf(*user_id))
        {
            wfrest::Json::Object it;
            it.push_back("level", level.level);
            it.push_back("users", level.users);
            levels.push_back(it);
        }
        out.push_back("levels", levels);

        int64_t total = affiliate->ordersCount(*user_id);
        std::vector<domain::AffiliateOrderRow> rows =
            affiliate->ordersOf(*user_id, page.offset(), page.page_size);

        wfrest::Json::Array items;
        for (const domain::AffiliateOrderRow &row : rows)
        {
            wfrest::Json::Object item;
            item.push_back("order_id", row.order_id);
            item.push_back("order_sn", row.order_sn_masked);
            item.push_back("order_amount", row.order_amount);
            item.push_back("separated", row.separated);
            if (row.separated)
            {
                item.push_back("money", row.money);
                item.push_back("point", row.point);
                item.push_back("separate_type", row.separate_type);
            }
            item.push_back("created_at",
                           shared::isoUtc(std::strtoll(row.created_at.c_str(), nullptr, 10)));
            items.push_back(item);
        }
        out.push_back("orders", api::listBody(page, total, items));

        api::send(req, resp, ApiResponse::ok(out));
    });
}

} // namespace ecshop::http
