#include "ecshop/http/MeRoutes.h"
#include "ecshop/http/AuthUtil.h"
#include "ecshop/http/HttpUtil.h"
#include "ecshop/infrastructure/SqlUserRepository.h"
#include "ecshop/shared/Password.h"
#include "ecshop/shared/TimeUtil.h"

#include <cstdlib>
#include <optional>
#include <string>

namespace ecshop::http {

using api::ApiError;
using api::ApiResponse;

static wfrest::Json userToJson(const domain::User &user)
{
    wfrest::Json::Object obj;
    obj.push_back("user_id", user.user_id);
    obj.push_back("username", user.username);
    obj.push_back("email", user.email);
    int64_t created = std::strtoll(user.created_at.c_str(), nullptr, 10);
    obj.push_back("created_at", shared::isoUtc(created));
    return obj;
}

void registerMeRoutes(wfrest::HttpServer &sv, std::shared_ptr<infra::Db> db)
{
    auto users = std::make_shared<infra::SqlUserRepository>(db);

    // GET /api/v1/me — current user profile
    sv.GET("/api/v1/me", [users](const wfrest::HttpReq *req, wfrest::HttpResp *resp)
    {
        api::ApiResponse err;
        std::optional<int64_t> user_id = api::authenticate(req, users, err);
        if (!user_id)
        {
            api::send(req, resp, err);
            return;
        }

        std::optional<domain::User> user = users->findById(*user_id);
        if (!user)
        {
            api::send(req, resp, ApiError::unauthenticated("invalid or expired session"));
            return;
        }

        api::send(req, resp, ApiResponse::ok(userToJson(*user)));
    });

    // PATCH /api/v1/me — update profile (email)
    sv.PATCH("/api/v1/me", [users](const wfrest::HttpReq *req, wfrest::HttpResp *resp)
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

        std::string email;
        if (!api::readStr(body, "email", email) || email.find('@') == std::string::npos ||
            email.size() > 120)
        {
            wfrest::Json::Object details;
            details.push_back("field", "email");
            api::send(req, resp,
                      ApiError::validationError("email must be a valid address", details));
            return;
        }

        if (!users->updateEmail(*user_id, email))
        {
            api::send(req, resp, ApiError::conflict("email_taken", "email is taken"));
            return;
        }

        std::optional<domain::User> user = users->findById(*user_id);
        if (!user)
        {
            api::send(req, resp, ApiError::unauthenticated("invalid or expired session"));
            return;
        }

        api::send(req, resp, ApiResponse::ok(userToJson(*user)));
    });

    // PATCH /api/v1/me/password — change password, revoke all sessions
    sv.PATCH("/api/v1/me/password",
             [users, db](const wfrest::HttpReq *req, wfrest::HttpResp *resp)
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

        std::string current_password, new_password;
        if (!api::readStr(body, "current_password", current_password) ||
            current_password.empty())
        {
            wfrest::Json::Object details;
            details.push_back("field", "current_password");
            api::send(req, resp,
                      ApiError::validationError("current_password is required", details));
            return;
        }
        if (!api::readStr(body, "new_password", new_password) || new_password.size() < 8 ||
            new_password.size() > 1024)
        {
            wfrest::Json::Object details;
            details.push_back("field", "new_password");
            api::send(req, resp,
                      ApiError::validationError("new_password must be 8-1024 bytes", details));
            return;
        }

        std::optional<std::string> old_hash = users->passwordHashOf(*user_id);
        if (!old_hash || !shared::verifyPassword(current_password, *old_hash))
        {
            api::send(req, resp,
                      ApiError::conflict("password_conflict", "current password is wrong"));
            return;
        }

        std::string new_hash = shared::hashPassword(new_password);
        bool updated = false;
        try
        {
            db->transaction([&] {
                updated = users->updatePasswordIfHashMatches(*user_id, *old_hash, new_hash);
                if (updated)
                    users->deleteSessionsOfUser(*user_id);
            });
        }
        catch (const infra::DbError &)
        {
            api::send(req, resp,
                      ApiError::conflict("password_conflict", "password changed concurrently"));
            return;
        }

        if (!updated)
        {
            api::send(req, resp,
                      ApiError::conflict("password_conflict", "password changed concurrently"));
            return;
        }

        api::send(req, resp, ApiResponse::noContent());
    });
}

} // namespace ecshop::http
