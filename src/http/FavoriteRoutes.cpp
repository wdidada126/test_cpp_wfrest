#include "ecshop/http/FavoriteRoutes.h"
#include "ecshop/http/AuthUtil.h"
#include "ecshop/http/HttpUtil.h"
#include "ecshop/infrastructure/SqlFavoriteRepository.h"
#include "ecshop/infrastructure/SqlUserRepository.h"
#include "ecshop/shared/TimeUtil.h"

#include <cstdlib>
#include <optional>
#include <vector>

namespace ecshop::http {

using api::ApiError;
using api::ApiResponse;

static wfrest::Json favoriteToJson(const domain::Favorite &favorite)
{
    wfrest::Json::Object obj;
    obj.push_back("id", favorite.rec_id);
    obj.push_back("goods_id", favorite.goods_id);
    obj.push_back("name", favorite.name);
    obj.push_back("price", favorite.price);
    obj.push_back("attention", favorite.attention);
    obj.push_back("add_time",
                  shared::isoUtc(std::strtoll(favorite.add_time.c_str(), nullptr, 10)));
    return obj;
}

void registerFavoriteRoutes(wfrest::HttpServer &sv, std::shared_ptr<infra::Db> db)
{
    auto users = std::make_shared<infra::SqlUserRepository>(db);
    auto favorites = std::make_shared<infra::SqlFavoriteRepository>(db);

    // GET /api/v1/me/favorites
    sv.GET("/api/v1/me/favorites",
           [users, favorites](const wfrest::HttpReq *req, wfrest::HttpResp *resp)
    {
        api::ApiResponse err;
        std::optional<int64_t> user_id = api::authenticate(req, users, err);
        if (!user_id)
        {
            api::send(req, resp, err);
            return;
        }

        std::vector<domain::Favorite> all = favorites->listOfUser(*user_id);

        wfrest::Json::Array items;
        for (const domain::Favorite &favorite : all)
            items.push_back(favoriteToJson(favorite));

        api::PageQuery page;
        api::send(req, resp,
                  ApiResponse::ok(api::listBody(page, static_cast<int64_t>(all.size()), items)));
    });

    // POST /api/v1/me/favorites — favorite a visible goods
    sv.POST("/api/v1/me/favorites",
            [users, favorites](const wfrest::HttpReq *req, wfrest::HttpResp *resp)
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

        domain::FavoriteAddResult result = favorites->add(*user_id, goods_id);
        if (result == domain::FavoriteAddResult::GoodsNotVisible)
        {
            api::send(req, resp, ApiError::notFound("goods not found"));
            return;
        }
        if (result == domain::FavoriteAddResult::Duplicate)
        {
            api::send(req, resp, ApiError::conflict("favorite_exists", "goods already favorited"));
            return;
        }

        wfrest::Json::Object out;
        out.push_back("goods_id", goods_id);
        out.push_back("attention", false);
        api::send(req, resp, ApiResponse::created(out));
    });

    // PATCH /api/v1/me/favorites/{id} — attention flag
    sv.PATCH("/api/v1/me/favorites/{id}",
             [users, favorites](const wfrest::HttpReq *req, wfrest::HttpResp *resp)
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

        bool attention = false;
        if (!api::readBool(body, "attention", attention))
        {
            wfrest::Json::Object details;
            details.push_back("field", "attention");
            api::send(req, resp, ApiError::validationError("attention must be a boolean", details));
            return;
        }

        if (!favorites->setAttention(*user_id, rec_id, attention))
        {
            api::send(req, resp, ApiError::notFound("favorite not found"));
            return;
        }

        wfrest::Json::Object out;
        out.push_back("id", rec_id);
        out.push_back("attention", attention);
        api::send(req, resp, ApiResponse::ok(out));
    });

    // DELETE /api/v1/me/favorites/{id}
    sv.DELETE("/api/v1/me/favorites/{id}",
              [users, favorites](const wfrest::HttpReq *req, wfrest::HttpResp *resp)
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

        if (!favorites->remove(*user_id, rec_id))
        {
            api::send(req, resp, ApiError::notFound("favorite not found"));
            return;
        }

        api::send(req, resp, ApiResponse::noContent());
    });
}

} // namespace ecshop::http
