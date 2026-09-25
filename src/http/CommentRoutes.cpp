#include "ecshop/http/CommentRoutes.h"
#include "ecshop/http/AuthUtil.h"
#include "ecshop/http/HttpUtil.h"
#include "ecshop/infrastructure/SqlCatalogRepository.h"
#include "ecshop/infrastructure/SqlCommentRepository.h"
#include "ecshop/infrastructure/SqlUserRepository.h"
#include "ecshop/shared/TimeUtil.h"

#include <cstdlib>
#include <optional>
#include <string>
#include <vector>

namespace ecshop::http {

using api::ApiError;
using api::ApiResponse;

static wfrest::Json commentToJson(const domain::Comment &comment)
{
    wfrest::Json::Object obj;
    obj.push_back("comment_id", comment.comment_id);
    obj.push_back("goods_id", comment.goods_id);
    obj.push_back("user_name", comment.user_name);
    obj.push_back("content", comment.content);
    obj.push_back("add_time",
                  shared::isoUtc(std::strtoll(comment.add_time.c_str(), nullptr, 10)));
    return obj;
}

void registerCommentRoutes(wfrest::HttpServer &sv, std::shared_ptr<infra::Db> db)
{
    auto users = std::make_shared<infra::SqlUserRepository>(db);
    auto catalog = std::make_shared<infra::SqlCatalogRepository>(db);
    auto comments = std::make_shared<infra::SqlCommentRepository>(db);

    // GET /api/v1/goods/{id}/comments — published comments
    sv.GET("/api/v1/goods/{id}/comments",
           [catalog, comments](const wfrest::HttpReq *req, wfrest::HttpResp *resp)
    {
        int64_t goods_id = 0;
        if (!api::parsePathId(req, "id", goods_id))
        {
            wfrest::Json::Object details;
            details.push_back("field", "id");
            api::send(req, resp, ApiError::validationError("id must be a positive integer", details));
            return;
        }

        api::PageQuery page;
        api::ApiResponse err;
        if (!api::parsePage(req, page, err))
        {
            api::send(req, resp, err);
            return;
        }

        std::optional<domain::Goods> goods = catalog->findVisibleGoods(goods_id);
        if (!goods)
        {
            api::send(req, resp, ApiError::notFound("goods not found"));
            return;
        }

        std::vector<domain::Comment> all =
            comments->listOfGoods(goods_id, page.offset(), page.page_size);

        wfrest::Json::Array items;
        for (const domain::Comment &comment : all)
            items.push_back(commentToJson(comment));

        api::send(req, resp,
                  ApiResponse::ok(api::listBody(page, comments->countOfGoods(goods_id), items)));
    });

    // POST /api/v1/goods/{id}/comments — create a comment (auto-published)
    sv.POST("/api/v1/goods/{id}/comments",
            [users, catalog, comments](const wfrest::HttpReq *req, wfrest::HttpResp *resp)
    {
        api::ApiResponse err;
        std::optional<int64_t> user_id = api::authenticate(req, users, err);
        if (!user_id)
        {
            api::send(req, resp, err);
            return;
        }

        int64_t goods_id = 0;
        if (!api::parsePathId(req, "id", goods_id))
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

        std::string content;
        if (!api::readStr(body, "content", content) || content.empty() || content.size() > 2000)
        {
            wfrest::Json::Object details;
            details.push_back("field", "content");
            api::send(req, resp,
                      ApiError::validationError("content must be 1-2000 bytes", details));
            return;
        }

        std::optional<domain::Goods> goods = catalog->findVisibleGoods(goods_id);
        if (!goods)
        {
            api::send(req, resp, ApiError::notFound("goods not found"));
            return;
        }

        std::optional<domain::User> user = users->findById(*user_id);
        if (!user)
        {
            api::send(req, resp, ApiError::unauthenticated("invalid or expired session"));
            return;
        }

        if (!comments->add(goods_id, *user_id, user->username, content))
        {
            api::send(req, resp, ApiError::notFound("goods not found"));
            return;
        }

        wfrest::Json::Object out;
        out.push_back("goods_id", goods_id);
        out.push_back("user_name", user->username);
        out.push_back("content", content);
        api::send(req, resp, ApiResponse::created(out));
    });
}

} // namespace ecshop::http
