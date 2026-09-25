#include "ecshop/http/MessageRoutes.h"
#include "ecshop/http/AuthUtil.h"
#include "ecshop/http/HttpUtil.h"
#include "ecshop/infrastructure/SqlMessageRepository.h"
#include "ecshop/infrastructure/SqlUserRepository.h"

#include <cstdlib>
#include <optional>
#include <string>

namespace ecshop::http {

using api::ApiError;
using api::ApiResponse;

static wfrest::Json messageToJson(const domain::Message &message)
{
    wfrest::Json::Object obj;
    obj.push_back("msg_id", message.msg_id);
    obj.push_back("username", message.username);
    obj.push_back("email", message.email);
    obj.push_back("title", message.title);
    obj.push_back("content", message.content);
    obj.push_back("type", message.type);
    obj.push_back("order_id", message.order_id);
    obj.push_back("created_at",
                  shared::isoUtc(std::strtoll(message.created_at.c_str(), nullptr, 10)));
    return obj;
}

void registerMessageRoutes(wfrest::HttpServer &sv, std::shared_ptr<infra::Db> db)
{
    auto users = std::make_shared<infra::SqlUserRepository>(db);
    auto messages = std::make_shared<infra::SqlMessageRepository>(db);

    // GET /api/v1/messages — public reviewed board messages
    sv.GET("/api/v1/messages", [messages](const wfrest::HttpReq *req, wfrest::HttpResp *resp)
    {
        api::PageQuery page;
        api::ApiResponse err;
        if (!api::parsePage(req, page, err))
        {
            api::send(req, resp, err);
            return;
        }

        domain::MessagePage result = messages->listPublic(page.offset(), page.page_size);

        wfrest::Json::Array items;
        for (const domain::Message &message : result.items)
            items.push_back(messageToJson(message));

        api::send(req, resp, ApiResponse::ok(api::listBody(page, result.total, items)));
    });

    // POST /api/v1/messages — anonymous or logged-in board message
    sv.POST("/api/v1/messages",
            [users, messages](const wfrest::HttpReq *req, wfrest::HttpResp *resp)
    {
        wfrest::Json body;
        api::ApiResponse err;
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

        // optional Bearer token binds the message to the current session
        int64_t user_id = 0;
        std::string default_username;
        std::optional<int64_t> session_user = api::authenticate(req, users, err);
        if (session_user)
        {
            user_id = *session_user;
            std::optional<domain::User> user = users->findById(user_id);
            if (!user)
            {
                api::send(req, resp, ApiError::unauthenticated("invalid or expired session"));
                return;
            }
            default_username = user->username;
        }

        std::string username, email, title;
        bool anonymous = false;
        api::readBool(body, "anonymous", anonymous);

        if (anonymous)
        {
            username = "anonymous";
        }
        else if (user_id > 0)
        {
            username = default_username;
        }

        if (body.has("username") &&
            (!api::readStr(body, "username", username) || username.size() > 60))
        {
            wfrest::Json::Object details;
            details.push_back("field", "username");
            api::send(req, resp,
                      ApiError::validationError("username must be at most 60 bytes", details));
            return;
        }
        if (username.empty())
            username = "anonymous";

        if (body.has("email") && (!api::readStr(body, "email", email) || email.size() > 60))
        {
            wfrest::Json::Object details;
            details.push_back("field", "email");
            api::send(req, resp,
                      ApiError::validationError("email must be at most 60 bytes", details));
            return;
        }

        if (body.has("title") && (!api::readStr(body, "title", title) || title.size() > 200))
        {
            wfrest::Json::Object details;
            details.push_back("field", "title");
            api::send(req, resp,
                      ApiError::validationError("title must be at most 200 bytes", details));
            return;
        }

        int64_t type = 0;
        if (body.has("type") && !api::readInt(body, "type", type))
        {
            wfrest::Json::Object details;
            details.push_back("field", "type");
            api::send(req, resp, ApiError::validationError("type must be an integer", details));
            return;
        }

        messages->add(user_id, username, email, type, title.empty() ? std::string("咨询") : title,
                      content);

        wfrest::Json::Object out;
        out.push_back("username", username);
        out.push_back("title", title.empty() ? std::string("咨询") : title);
        out.push_back("content", content);
        api::send(req, resp, ApiResponse::created(out));
    });

    // GET /api/v1/me/messages — own top-level messages + first reply
    sv.GET("/api/v1/me/messages",
           [users, messages](const wfrest::HttpReq *req, wfrest::HttpResp *resp)
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

        domain::MessagePage result =
            messages->listOfUser(*user_id, page.offset(), page.page_size);

        wfrest::Json::Array items;
        for (const domain::Message &message : result.items)
        {
            wfrest::Json::Object item;
            item.push_back("msg_id", message.msg_id);
            item.push_back("title", message.title);
            item.push_back("content", message.content);
            item.push_back("order_id", message.order_id);
            item.push_back("created_at",
                           shared::isoUtc(std::strtoll(message.created_at.c_str(), nullptr, 10)));

            if (message.has_reply)
            {
                wfrest::Json::Object reply;
                reply.push_back("username", message.reply_username);
                reply.push_back("content", message.reply_content);
                reply.push_back("created_at",
                                shared::isoUtc(std::strtoll(message.reply_time.c_str(), nullptr, 10)));
                item.push_back("reply", reply);
            }
            items.push_back(item);
        }

        api::send(req, resp, ApiResponse::ok(api::listBody(page, result.total, items)));
    });
}

} // namespace ecshop::http
