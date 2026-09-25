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
}

} // namespace ecshop::http
