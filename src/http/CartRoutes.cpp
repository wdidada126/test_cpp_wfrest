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
}

} // namespace ecshop::http
