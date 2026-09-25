#include "ecshop/http/TagRoutes.h"
#include "ecshop/http/AuthUtil.h"
#include "ecshop/http/HttpUtil.h"
#include "ecshop/infrastructure/SqlTagRepository.h"
#include "ecshop/infrastructure/SqlUserRepository.h"

#include <optional>
#include <string>
#include <vector>

namespace ecshop::http {

using api::ApiError;
using api::ApiResponse;

// "C++,商城" -> {"C++","商城"}; 1-10 non-empty words, each <= 255 bytes
static bool parseTagWords(const std::string &raw, std::vector<std::string> &out,
                          ApiResponse &err)
{
    auto fail = [&err](const char *message) {
        wfrest::Json::Object details;
        details.push_back("field", "tag");
        err = ApiError::validationError(message, details);
        return false;
    };

    size_t start = 0;
    while (start <= raw.size())
    {
        size_t comma = raw.find(',', start);
        std::string word =
            raw.substr(start, comma == std::string::npos ? std::string::npos : comma - start);

        if (word.empty())
            return fail("tag must contain non-empty words");
        if (word.size() > 255)
            return fail("each tag must be at most 255 bytes");
        out.push_back(word);

        if (out.size() > 10)
            return fail("tag accepts 1-10 words");
        if (comma == std::string::npos)
            break;
        start = comma + 1;
    }

    if (out.empty())
        return fail("tag is required");
    return true;
}

static wfrest::Json tagStatsToJson(const std::vector<domain::TagCount> &stats)
{
    wfrest::Json::Array items;
    for (const domain::TagCount &item : stats)
    {
        wfrest::Json::Object obj;
        obj.push_back("word", item.word);
        obj.push_back("count", item.count);
        items.push_back(obj);
    }
    wfrest::Json::Object out;
    out.push_back("items", items);
    return out;
}

void registerTagRoutes(wfrest::HttpServer &sv, std::shared_ptr<infra::Db> db)
{
    auto users = std::make_shared<infra::SqlUserRepository>(db);
    auto tags = std::make_shared<infra::SqlTagRepository>(db);

    // POST /api/v1/goods/{id}/tags — {"tag":"C++,商城"}
    sv.POST("/api/v1/goods/{id}/tags",
            [users, tags](const wfrest::HttpReq *req, wfrest::HttpResp *resp)
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

        std::string raw;
        if (!api::readStr(body, "tag", raw))
        {
            wfrest::Json::Object details;
            details.push_back("field", "tag");
            api::send(req, resp, ApiError::validationError("tag is required", details));
            return;
        }

        std::vector<std::string> words;
        if (!parseTagWords(raw, words, err))
        {
            api::send(req, resp, err);
            return;
        }

        std::vector<domain::TagCount> stats;
        if (!tags->addTags(*user_id, goods_id, words, stats))
        {
            api::send(req, resp, ApiError::notFound("goods not found"));
            return;
        }

        api::send(req, resp, ApiResponse::created(tagStatsToJson(stats)));
    });

    // GET /api/v1/me/tags — current user's tag words with usage counts
    sv.GET("/api/v1/me/tags", [users, tags](const wfrest::HttpReq *req, wfrest::HttpResp *resp)
    {
        api::ApiResponse err;
        std::optional<int64_t> user_id = api::authenticate(req, users, err);
        if (!user_id)
        {
            api::send(req, resp, err);
            return;
        }

        api::send(req, resp, ApiResponse::ok(tagStatsToJson(tags->listOfUser(*user_id))));
    });

    // DELETE /api/v1/me/tags — {"tag":"C++"}, idempotent, own rows only
    sv.DELETE("/api/v1/me/tags", [users, tags](const wfrest::HttpReq *req, wfrest::HttpResp *resp)
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

        std::string word;
        if (!api::readStr(body, "tag", word) || word.empty() || word.size() > 255)
        {
            wfrest::Json::Object details;
            details.push_back("field", "tag");
            api::send(req, resp,
                      ApiError::validationError("tag must be 1-255 bytes", details));
            return;
        }

        tags->removeTag(*user_id, word);
        api::send(req, resp, ApiResponse::noContent());
    });
}

} // namespace ecshop::http
