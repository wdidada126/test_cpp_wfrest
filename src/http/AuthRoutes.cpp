#include "ecshop/http/AuthRoutes.h"
#include "ecshop/http/AuthUtil.h"
#include "ecshop/http/HttpUtil.h"
#include "ecshop/infrastructure/SqlUserRepository.h"
#include "ecshop/shared/Password.h"

#include <optional>

namespace ecshop::http {

using api::ApiError;
using api::ApiResponse;

static bool looksLikeEmail(const std::string &email)
{
    size_t at = email.find('@');
    return at != std::string::npos && at > 0 && at + 1 < email.size() &&
           email.find('@', at + 1) == std::string::npos;
}

void registerAuthRoutes(wfrest::HttpServer &sv, std::shared_ptr<infra::Db> db)
{
    auto users = std::make_shared<infra::SqlUserRepository>(db);

    // POST /api/v1/auth/register
    sv.POST("/api/v1/auth/register", [users](const wfrest::HttpReq *req, wfrest::HttpResp *resp)
    {
        wfrest::Json body;
        api::ApiResponse err;
        if (!api::parseJsonBody(req, body, err))
        {
            api::send(req, resp, err);
            return;
        }

        std::string username, email, password;
        if (!api::readStr(body, "username", username) || username.empty() ||
            username.size() > 60)
        {
            wfrest::Json::Object details;
            details.push_back("field", "username");
            api::send(req, resp,
                      ApiError::validationError("username must be 1-60 bytes", details));
            return;
        }
        if (!api::readStr(body, "email", email) || !looksLikeEmail(email) ||
            email.size() > 120)
        {
            wfrest::Json::Object details;
            details.push_back("field", "email");
            api::send(req, resp,
                      ApiError::validationError("email must be a valid address", details));
            return;
        }
        if (!api::readStr(body, "password", password) || password.size() < 8 ||
            password.size() > 1024)
        {
            wfrest::Json::Object details;
            details.push_back("field", "password");
            api::send(req, resp,
                      ApiError::validationError("password must be 8-1024 bytes", details));
            return;
        }

        bool agreement = false;
        if (!api::readBool(body, "agreement_accepted", agreement) || !agreement)
        {
            wfrest::Json::Object details;
            details.push_back("field", "agreement_accepted");
            api::send(req, resp,
                      ApiError::validationError("agreement_accepted must be true", details));
            return;
        }

        if (users->findByUsername(username))
        {
            api::send(req, resp, ApiError::conflict("username_taken", "username is taken"));
            return;
        }
        if (users->findByEmail(email))
        {
            api::send(req, resp, ApiError::conflict("email_taken", "email is taken"));
            return;
        }

        int64_t user_id = 0;
        try
        {
            user_id = users->createUser(username, email, shared::hashPassword(password));
        }
        catch (const infra::DbError &)
        {
            // unique constraint race: registration relies on the DB, not the check
            api::send(req, resp, ApiError::conflict("username_taken", "username or email is taken"));
            return;
        }

        std::string token = api::issueToken(users, user_id);

        wfrest::Json::Object out;
        out.push_back("user_id", user_id);
        out.push_back("username", username);
        out.push_back("access_token", token);
        api::send(req, resp, ApiResponse::created(out));
    });

    // POST /api/v1/auth/availability/username — hint only, DB unique keys decide
    sv.POST("/api/v1/auth/availability/username",
            [users](const wfrest::HttpReq *req, wfrest::HttpResp *resp)
    {
        wfrest::Json body;
        api::ApiResponse err;
        if (!api::parseJsonBody(req, body, err))
        {
            api::send(req, resp, err);
            return;
        }

        std::string username;
        if (!api::readStr(body, "username", username) || username.empty())
        {
            wfrest::Json::Object details;
            details.push_back("field", "username");
            api::send(req, resp, ApiError::validationError("username is required", details));
            return;
        }

        wfrest::Json::Object out;
        out.push_back("available", !users->findByUsername(username).has_value());
        api::send(req, resp, ApiResponse::ok(out));
    });

    // POST /api/v1/auth/availability/email — hint only, DB unique keys decide
    sv.POST("/api/v1/auth/availability/email",
            [users](const wfrest::HttpReq *req, wfrest::HttpResp *resp)
    {
        wfrest::Json body;
        api::ApiResponse err;
        if (!api::parseJsonBody(req, body, err))
        {
            api::send(req, resp, err);
            return;
        }

        std::string email;
        if (!api::readStr(body, "email", email) || !looksLikeEmail(email))
        {
            wfrest::Json::Object details;
            details.push_back("field", "email");
            api::send(req, resp, ApiError::validationError("email is required", details));
            return;
        }

        wfrest::Json::Object out;
        out.push_back("available", !users->findByEmail(email).has_value());
        api::send(req, resp, ApiResponse::ok(out));
    });

    // POST /api/v1/auth/login
    sv.POST("/api/v1/auth/login", [users](const wfrest::HttpReq *req, wfrest::HttpResp *resp)
    {
        wfrest::Json body;
        api::ApiResponse err;
        if (!api::parseJsonBody(req, body, err))
        {
            api::send(req, resp, err);
            return;
        }

        std::string username, password;
        if (!api::readStr(body, "username", username) || username.empty() ||
            !api::readStr(body, "password", password) || password.empty())
        {
            wfrest::Json::Object details;
            details.push_back("field", "username");
            api::send(req, resp,
                      ApiError::validationError("username and password are required", details));
            return;
        }

        std::optional<domain::User> user = users->findByUsername(username);
        std::optional<std::string> hash =
            user ? users->passwordHashOf(user->user_id) : std::nullopt;
        if (!user || !hash || !shared::verifyPassword(password, *hash))
        {
            api::send(req, resp, ApiError::unauthenticated("invalid username or password"));
            return;
        }

        std::string token = api::issueToken(users, user->user_id);

        wfrest::Json::Object out;
        out.push_back("user_id", user->user_id);
        out.push_back("username", user->username);
        out.push_back("access_token", token);
        api::send(req, resp, ApiResponse::ok(out));
    });
}

} // namespace ecshop::http
