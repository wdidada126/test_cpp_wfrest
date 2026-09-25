#include "ecshop/http/ArticleRoutes.h"
#include "ecshop/http/HttpUtil.h"
#include "ecshop/infrastructure/SqlArticleRepository.h"

#include <optional>

namespace ecshop::http {

using api::ApiError;
using api::ApiResponse;

static wfrest::Json articleToJson(const domain::Article &article)
{
    wfrest::Json::Object obj;
    obj.push_back("article_id", article.article_id);
    obj.push_back("cat_id", article.cat_id);
    obj.push_back("title", article.title);
    obj.push_back("author", article.author);
    obj.push_back("description", article.description);
    obj.push_back("content", article.content);
    obj.push_back("keywords", article.keywords);
    return obj;
}

void registerArticleRoutes(wfrest::HttpServer &sv, std::shared_ptr<infra::Db> db)
{
    auto repo = std::make_shared<infra::SqlArticleRepository>(db);

    // GET /api/v1/articles/{id} — public article detail
    sv.GET("/api/v1/articles/{id}", [repo](const wfrest::HttpReq *req, wfrest::HttpResp *resp)
    {
        int64_t article_id = 0;
        if (!api::parsePathId(req, "id", article_id))
        {
            wfrest::Json::Object details;
            details.push_back("field", "id");
            api::send(req, resp, ApiError::validationError("id must be a positive integer", details));
            return;
        }

        std::optional<domain::Article> article = repo->findOpenArticle(article_id);
        if (!article)
        {
            api::send(req, resp, ApiError::notFound("article not found"));
            return;
        }

        api::send(req, resp, ApiResponse::ok(articleToJson(*article)));
    });
}

} // namespace ecshop::http
