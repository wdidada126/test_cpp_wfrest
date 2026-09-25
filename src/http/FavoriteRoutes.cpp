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
}

} // namespace ecshop::http
