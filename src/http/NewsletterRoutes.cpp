#include "ecshop/http/NewsletterRoutes.h"
#include "ecshop/http/HttpUtil.h"
#include "ecshop/infrastructure/SqlNewsletterRepository.h"
#include "ecshop/shared/Password.h"
#include "ecshop/shared/TimeUtil.h"

#include <optional>
#include <string>

namespace ecshop::http {

using api::ApiError;
using api::ApiResponse;

// shared body/token helpers for the four newsletter URLs
static bool parseNewsletterEmail(const wfrest::Json &body, std::string &email, ApiResponse &err)
{
    if (!api::readStr(body, "email", email) || email.empty() || email.size() > 120 ||
        email.find('@') == std::string::npos)
    {
        wfrest::Json::Object details;
        details.push_back("field", "email");
        err = ApiError::validationError("email must be a valid address", details);
        return false;
    }
    return true;
}

static bool parseConfirmToken(const wfrest::Json &body, std::string &token, ApiResponse &err)
{
    if (!api::readStr(body, "token", token) || token.empty() || token.size() > 128)
    {
        wfrest::Json::Object details;
        details.push_back("field", "token");
        err = ApiError::validationError("token is required", details);
        return false;
    }
    return true;
}

static api::ApiResponse acceptedStatus(const char *status)
{
    wfrest::Json::Object out;
    out.push_back("status", std::string(status));
    return api::ApiResponse::accepted(out);
}

void registerNewsletterRoutes(wfrest::HttpServer &sv, std::shared_ptr<infra::Db> db)
{
    auto newsletter = std::make_shared<infra::SqlNewsletterRepository>(db);

    // POST /api/v1/newsletter-subscriptions — request subscribe (202)
    sv.POST("/api/v1/newsletter-subscriptions",
            [newsletter](const wfrest::HttpReq *req, wfrest::HttpResp *resp)
    {
        wfrest::Json body;
        api::ApiResponse err;
        if (!api::parseJsonBody(req, body, err))
        {
            api::send(req, resp, err);
            return;
        }

        std::string email;
        if (!parseNewsletterEmail(body, email, err))
        {
            api::send(req, resp, err);
            return;
        }

        std::string token = shared::randomTokenHex(32);
        bool queued = newsletter->requestSubscribe(email, shared::sha256Hex(token), token,
                                                   shared::nowUnix() + 24 * 3600);
        (void)queued; // already-subscribed is an idempotent 202 as well
        api::send(req, resp, acceptedStatus("accepted"));
    });

    // DELETE /api/v1/newsletter-subscriptions — request unsubscribe (202)
    sv.DELETE("/api/v1/newsletter-subscriptions",
              [newsletter](const wfrest::HttpReq *req, wfrest::HttpResp *resp)
    {
        wfrest::Json body;
        api::ApiResponse err;
        if (!api::parseJsonBody(req, body, err))
        {
            api::send(req, resp, err);
            return;
        }

        std::string email;
        if (!parseNewsletterEmail(body, email, err))
        {
            api::send(req, resp, err);
            return;
        }

        std::string token = shared::randomTokenHex(32);
        (void)newsletter->requestUnsubscribe(email, shared::sha256Hex(token), token,
                                             shared::nowUnix() + 24 * 3600);
        api::send(req, resp, acceptedStatus("accepted"));
    });

    // POST /api/v1/newsletter-subscriptions/confirm — consume subscribe token
    sv.POST("/api/v1/newsletter-subscriptions/confirm",
            [newsletter](const wfrest::HttpReq *req, wfrest::HttpResp *resp)
    {
        wfrest::Json body;
        api::ApiResponse err;
        if (!api::parseJsonBody(req, body, err))
        {
            api::send(req, resp, err);
            return;
        }

        std::string token;
        if (!parseConfirmToken(body, token, err))
        {
            api::send(req, resp, err);
            return;
        }

        if (!newsletter->confirmSubscribe(shared::sha256Hex(token), shared::nowUnix()))
        {
            api::send(req, resp,
                      ApiError::conflict("token_invalid", "token is invalid or expired"));
            return;
        }

        api::send(req, resp, ApiResponse::noContent());
    });

    // POST /api/v1/newsletter-unsubscriptions/confirm — consume unsubscribe token
    sv.POST("/api/v1/newsletter-unsubscriptions/confirm",
            [newsletter](const wfrest::HttpReq *req, wfrest::HttpResp *resp)
    {
        wfrest::Json body;
        api::ApiResponse err;
        if (!api::parseJsonBody(req, body, err))
        {
            api::send(req, resp, err);
            return;
        }

        std::string token;
        if (!parseConfirmToken(body, token, err))
        {
            api::send(req, resp, err);
            return;
        }

        if (!newsletter->confirmUnsubscribe(shared::sha256Hex(token), shared::nowUnix()))
        {
            api::send(req, resp,
                      ApiError::conflict("token_invalid", "token is invalid or expired"));
            return;
        }

        api::send(req, resp, ApiResponse::noContent());
    });
}

} // namespace ecshop::http
