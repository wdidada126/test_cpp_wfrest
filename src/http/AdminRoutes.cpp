#include "ecshop/http/AdminRoutes.h"
#include "ecshop/http/HttpUtil.h"
#include "ecshop/infrastructure/SqlAdminRepository.h"
#include "ecshop/shared/Password.h"
#include "ecshop/shared/TimeUtil.h"

#include <cstdlib>
#include <optional>
#include <string>

namespace ecshop::http {

using api::ApiError;
using api::ApiResponse;

namespace {

std::optional<domain::AdminUser> adminAuth(
    const wfrest::HttpReq *req, const std::shared_ptr<infra::SqlAdminRepository> &admins,
    ApiResponse &err)
{
    const std::string &header = req->header("Authorization");
    static const std::string kPrefix = "Bearer ";
    if (header.compare(0, kPrefix.size(), kPrefix) != 0 || header.size() <= kPrefix.size())
    {
        err = ApiError::unauthenticated("missing admin Bearer token");
        return std::nullopt;
    }

    std::optional<int64_t> admin_id =
        admins->adminIdOfTokenHash(shared::sha256Hex(header.substr(kPrefix.size())));
    if (!admin_id)
    {
        err = ApiError::unauthenticated("invalid or expired admin session");
        return std::nullopt;
    }

    std::optional<domain::AdminUser> admin = admins->findById(*admin_id);
    if (!admin)
    {
        err = ApiError::unauthenticated("invalid or expired admin session");
        return std::nullopt;
    }
    return admin;
}

} // namespace

void registerAdminRoutes(wfrest::HttpServer &sv, std::shared_ptr<infra::Db> db,
                         const app::AppConfig &cfg)
{
    auto admins = std::make_shared<infra::SqlAdminRepository>(db);

    // seed the bootstrap admin from the environment (dev fallback admin/...)
    admins->ensureSchema();
    const char *seed_user = std::getenv("ECSHOP_ADMIN_USER");
    const char *seed_pass = std::getenv("ECSHOP_ADMIN_PASSWORD");
    std::string username = seed_user && *seed_user ? seed_user : "admin";
    std::string password = seed_pass && *seed_pass ? seed_pass : "admin123456";
    std::string seed_role = "super";
    const char *seed_role_env = std::getenv("ECSHOP_ADMIN_ROLE");
    if (seed_role_env && *seed_role_env)
        seed_role = seed_role_env;
    admins->ensureAdmin(username, shared::hashPassword(password), seed_role);

    // POST /api/v1/admin/login
    sv.POST("/api/v1/admin/login",
            [admins](const wfrest::HttpReq *req, wfrest::HttpResp *resp)
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

        std::optional<domain::AdminUser> admin = admins->findByUsername(username);
        std::optional<std::string> hash =
            admin ? admins->passwordHashOf(admin->admin_id) : std::nullopt;
        if (!admin || !hash || !shared::verifyPassword(password, *hash))
        {
            api::send(req, resp, ApiError::unauthenticated("invalid admin credentials"));
            return;
        }

        std::string token = shared::randomTokenHex(32);
        admins->createSession(shared::sha256Hex(token), admin->admin_id);
        admins->writeLog(admin->admin_id, "login",
                         task_of(resp)->peer_addr(), task_of(resp)->peer_addr());

        wfrest::Json::Object out;
        out.push_back("admin_id", admin->admin_id);
        out.push_back("username", admin->username);
        out.push_back("role", admin->role);
        out.push_back("access_token", token);
        api::send(req, resp, ApiResponse::ok(out));
    });

    // POST /api/v1/admin/logout
    sv.POST("/api/v1/admin/logout",
            [admins](const wfrest::HttpReq *req, wfrest::HttpResp *resp)
    {
        ApiResponse err;
        std::optional<domain::AdminUser> admin = adminAuth(req, admins, err);
        if (!admin)
        {
            api::send(req, resp, err);
            return;
        }

        std::string token = req->header("Authorization").substr(std::string("Bearer ").size());
        admins->deleteSession(shared::sha256Hex(token));
        api::send(req, resp, ApiResponse::noContent());
    });

    // GET /api/v1/admin/session — current admin identity
    sv.GET("/api/v1/admin/session",
           [admins](const wfrest::HttpReq *req, wfrest::HttpResp *resp)
    {
        ApiResponse err;
        std::optional<domain::AdminUser> admin = adminAuth(req, admins, err);
        if (!admin)
        {
            api::send(req, resp, err);
            return;
        }

        wfrest::Json::Object out;
        out.push_back("admin_id", admin->admin_id);
        out.push_back("username", admin->username);
        out.push_back("role", admin->role);
        api::send(req, resp, ApiResponse::ok(out));
    });
}

} // namespace ecshop::http
