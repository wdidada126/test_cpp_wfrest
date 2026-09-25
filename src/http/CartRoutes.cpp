#include "ecshop/http/CartRoutes.h"
#include "ecshop/http/AuthUtil.h"
#include "ecshop/http/HttpUtil.h"
#include "ecshop/infrastructure/SqlCartRepository.h"
#include "ecshop/infrastructure/SqlUserRepository.h"

#include <optional>
#include <vector>

namespace ecshop::http {

using api::ApiError;
using api::ApiResponse;

static wfrest::Json cartItemToJson(const domain::CartItem &item)
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

void registerCartRoutes(wfrest::HttpServer &sv, std::shared_ptr<infra::Db> db)
{
    auto users = std::make_shared<infra::SqlUserRepository>(db);
    auto cart = std::make_shared<infra::SqlCartRepository>(db);

    // GET /api/v1/me/cart — {"items":[...],"total_quantity":n} per docs/03
    sv.GET("/api/v1/me/cart", [users, cart](const wfrest::HttpReq *req, wfrest::HttpResp *resp)
    {
        api::ApiResponse err;
        std::optional<int64_t> user_id = api::authenticate(req, users, err);
        if (!user_id)
        {
            api::send(req, resp, err);
            return;
        }

        std::vector<domain::CartItem> all = cart->listOfUser(*user_id);

        wfrest::Json::Array items;
        int64_t total_quantity = 0;
        for (const domain::CartItem &item : all)
        {
            items.push_back(cartItemToJson(item));
            total_quantity += item.quantity;
        }

        wfrest::Json::Object out;
        out.push_back("items", items);
        out.push_back("total_quantity", total_quantity);
        api::send(req, resp, ApiResponse::ok(out));
    });

    // POST /api/v1/me/cart — add item (accumulates quantity on repeat)
    sv.POST("/api/v1/me/cart", [users, cart](const wfrest::HttpReq *req, wfrest::HttpResp *resp)
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

        int64_t goods_id = 0;
        if (!api::readInt(body, "goods_id", goods_id) || goods_id <= 0)
        {
            wfrest::Json::Object details;
            details.push_back("field", "goods_id");
            api::send(req, resp,
                      ApiError::validationError("goods_id must be a positive integer", details));
            return;
        }

        int64_t quantity = 0;
        if (!api::readInt(body, "quantity", quantity))
        {
            wfrest::Json::Object details;
            details.push_back("field", "quantity");
            api::send(req, resp, ApiError::validationError("quantity is required", details));
            return;
        }
        if (quantity < 1 || quantity > 999)
        {
            wfrest::Json::Object details;
            details.push_back("field", "quantity");
            api::send(req, resp,
                      ApiError::validationError("quantity must be between 1 and 999", details));
            return;
        }

        domain::CartItem item;
        domain::CartAddResult result = cart->add(*user_id, goods_id, quantity, item);
        if (result == domain::CartAddResult::GoodsNotVisible)
        {
            api::send(req, resp, ApiError::notFound("goods not found"));
            return;
        }

        api::send(req, resp, ApiResponse::created(cartItemToJson(item)));
    });

    // PATCH /api/v1/me/cart/{id} — quantity with optimistic version lock
    sv.PATCH("/api/v1/me/cart/{id}", [users, cart](const wfrest::HttpReq *req,
                                                   wfrest::HttpResp *resp)
    {
        api::ApiResponse err;
        std::optional<int64_t> user_id = api::authenticate(req, users, err);
        if (!user_id)
        {
            api::send(req, resp, err);
            return;
        }

        int64_t rec_id = 0;
        if (!api::parsePathId(req, "id", rec_id))
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

        int64_t quantity = 0;
        if (!api::readInt(body, "quantity", quantity) || quantity < 1 || quantity > 999)
        {
            wfrest::Json::Object details;
            details.push_back("field", "quantity");
            api::send(req, resp,
                      ApiError::validationError("quantity must be between 1 and 999", details));
            return;
        }

        int64_t version = 0;
        if (!api::readInt(body, "version", version) || version < 1)
        {
            wfrest::Json::Object details;
            details.push_back("field", "version");
            api::send(req, resp, ApiError::validationError("version is required", details));
            return;
        }

        std::optional<bool> updated = cart->updateQuantity(*user_id, rec_id, quantity, version);
        if (!updated)
        {
            api::send(req, resp, ApiError::notFound("cart item not found"));
            return;
        }
        if (!*updated)
        {
            api::send(req, resp,
                      ApiError::conflict("version_conflict", "cart item version is stale"));
            return;
        }

        api::send(req, resp, ApiResponse::noContent());
    });
}

} // namespace ecshop::http
