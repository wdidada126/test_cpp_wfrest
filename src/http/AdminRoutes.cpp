#include "ecshop/http/AdminRoutes.h"
#include "ecshop/http/HttpUtil.h"
#include "ecshop/infrastructure/SqlAdminGoodsRepository.h"
#include "ecshop/infrastructure/SqlAdminRepository.h"
#include "ecshop/shared/Money.h"
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

static wfrest::Json adminGoodsToJson(const domain::AdminGoodsRow &goods)
{
    wfrest::Json::Object obj;
    obj.push_back("goods_id", goods.goods_id);
    obj.push_back("goods_sn", goods.goods_sn);
    obj.push_back("name", goods.name);
    obj.push_back("brief", goods.brief);
    obj.push_back("price", goods.price);
    obj.push_back("market_price", goods.market_price);
    obj.push_back("currency", "CNY");
    obj.push_back("stock", goods.stock);
    obj.push_back("cat_id", goods.cat_id);
    obj.push_back("brand_id", goods.brand_id);
    obj.push_back("is_on_sale", goods.is_on_sale);
    obj.push_back("is_delete", goods.is_delete);
    obj.push_back("is_best", goods.is_best);
    obj.push_back("is_new", goods.is_new);
    obj.push_back("is_hot", goods.is_hot);
    obj.push_back("is_promote", goods.is_promote);
    return obj;
}

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

    auto goods = std::make_shared<infra::SqlAdminGoodsRepository>(db);

    // GET /api/v1/admin/goods?q=&page=&page_size= — all goods, unfiltered
    sv.GET("/api/v1/admin/goods",
           [admins, goods](const wfrest::HttpReq *req, wfrest::HttpResp *resp)
    {
        api::ApiResponse err;
        std::optional<domain::AdminUser> admin = adminAuth(req, admins, err);
        if (!admin)
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

        domain::AdminGoodsPage result = goods->list(req->query("q"), page.offset(), page.page_size);

        wfrest::Json::Array items;
        for (const domain::AdminGoodsRow &row : result.items)
            items.push_back(adminGoodsToJson(row));

        api::send(req, resp, ApiResponse::ok(api::listBody(page, result.total, items)));
    });

    // POST /api/v1/admin/goods — create goods; 201 with created row
    sv.POST("/api/v1/admin/goods",
            [admins, goods, db](const wfrest::HttpReq *req, wfrest::HttpResp *resp)
    {
        api::ApiResponse err;
        std::optional<domain::AdminUser> admin = adminAuth(req, admins, err);
        if (!admin)
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

        domain::AdminGoodsRow row;
        std::string description;

        std::string goods_sn;
        if (body.has("goods_sn") && !api::readStr(body, "goods_sn", goods_sn))
        {
            wfrest::Json::Object details;
            details.push_back("field", "goods_sn");
            api::send(req, resp,
                      ApiError::validationError("goods_sn must be a string", details));
            return;
        }
        if (body.has("goods_sn") && goods_sn.size() > 60)
        {
            wfrest::Json::Object details;
            details.push_back("field", "goods_sn");
            api::send(req, resp,
                      ApiError::validationError("goods_sn must be at most 60 bytes", details));
            return;
        }
        row.goods_sn = goods_sn;

        if (!api::readStr(body, "name", row.name) || row.name.empty() || row.name.size() > 255)
        {
            wfrest::Json::Object details;
            details.push_back("field", "name");
            api::send(req, resp,
                      ApiError::validationError("name must be 1-255 bytes", details));
            return;
        }

        if (body.has("brief"))
        {
            std::string brief;
            if (!api::readStr(body, "brief", brief) || brief.size() > 255)
            {
                wfrest::Json::Object details;
                details.push_back("field", "brief");
                api::send(req, resp,
                          ApiError::validationError("brief must be at most 255 bytes", details));
                return;
            }
            row.brief = brief;
        }

        if (body.has("description") && !api::readStr(body, "description", description))
        {
            wfrest::Json::Object details;
            details.push_back("field", "description");
            api::send(req, resp,
                      ApiError::validationError("description must be a string", details));
            return;
        }

        // prices default to zero when not provided; values are decimal strings
        row.price = "0.00";
        if (body.has("price"))
        {
            if (!api::readStr(body, "price", row.price))
            {
                wfrest::Json::Object details;
                details.push_back("field", "price");
                api::send(req, resp,
                          ApiError::validationError("price must be a decimal string", details));
                return;
            }
        }
        row.market_price = "0.00";
        if (body.has("market_price"))
        {
            if (!api::readStr(body, "market_price", row.market_price))
            {
                wfrest::Json::Object details;
                details.push_back("field", "market_price");
                api::send(req, resp,
                          ApiError::validationError("market_price must be a decimal string",
                                                    details));
                return;
            }
        }

        if (body.has("stock") && !api::readInt(body, "stock", row.stock) || row.stock < 0)
        {
            wfrest::Json::Object details;
            details.push_back("field", "stock");
            api::send(req, resp,
                      ApiError::validationError("stock must be a non-negative integer", details));
            return;
        }
        if (body.has("cat_id") && !api::readInt(body, "cat_id", row.cat_id) || row.cat_id < 0)
        {
            wfrest::Json::Object details;
            details.push_back("field", "cat_id");
            api::send(req, resp, ApiError::validationError("cat_id must be an integer", details));
            return;
        }
        if (body.has("brand_id") && !api::readInt(body, "brand_id", row.brand_id) ||
            row.brand_id < 0)
        {
            wfrest::Json::Object details;
            details.push_back("field", "brand_id");
            api::send(req, resp, ApiError::validationError("brand_id must be an integer", details));
            return;
        }

        row.is_on_sale = true;
        if (body.has("is_on_sale") && !api::readBool(body, "is_on_sale", row.is_on_sale))
        {
            wfrest::Json::Object details;
            details.push_back("field", "is_on_sale");
            api::send(req, resp, ApiError::validationError("is_on_sale must be a boolean", details));
            return;
        }

        // overlap: libsqlite/mysql both enforce a unique goods_sn when set
        int64_t created_id = 0;
        std::string final_sn = row.goods_sn.empty()
                                   ? "AUTO-" + std::to_string(rand()) // keep <= 60 bytes
                                   : row.goods_sn;
        row.goods_sn = final_sn;
        description = description.empty() ? "" : description;

        try
        {
            created_id = goods->create(row, description);
        }
        catch (const infra::DbError &)
        {
            api::send(req, resp, ApiError::conflict("goods_sn_conflict", "goods_sn is taken"));
            return;
        }

        std::optional<domain::AdminGoodsRow> created = goods->find(created_id);
        if (!created)
        {
            api::send(req, resp, ApiError::internalError("goods creation failed"));
            return;
        }

        admins->writeLog(admin->admin_id, "create_goods",
                         "goods_id=" + std::to_string(created_id),
                         task_of(resp)->peer_addr());
        api::send(req, resp, ApiResponse::created(adminGoodsToJson(*created)));
    });

    // PATCH /api/v1/admin/goods/{id} — optional-field patch, audit logged
    sv.PATCH("/api/v1/admin/goods/{id}",
             [admins, goods, db](const wfrest::HttpReq *req, wfrest::HttpResp *resp)
    {
        api::ApiResponse err;
        std::optional<domain::AdminUser> admin = adminAuth(req, admins, err);
        if (!admin)
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

        domain::AdminGoodsPatch patch;
        std::string value;

        auto checkStr = [&](const char *key, std::optional<std::string> &target,
                            size_t max_len) -> bool {
            if (!body.has(key))
                return true;
            if (!api::readStr(body, key, value) || value.size() > max_len)
            {
                wfrest::Json::Object details;
                details.push_back("field", std::string(key));
                err = ApiError::validationError(std::string(key) + " must be at most " +
                                                std::to_string(max_len) + " bytes",
                                                details);
                return false;
            }
            target = value;
            return true;
        };
        if (!checkStr("name", patch.name, 255) || !checkStr("brief", patch.brief, 255) ||
            !checkStr("description", patch.description, 1 << 16))
        {
            api::send(req, resp, err);
            return;
        }

        auto checkMoney = [&](const char *key, std::optional<std::string> &target) -> bool {
            if (!body.has(key))
                return true;
            if (!api::readStr(body, key, value))
            {
                wfrest::Json::Object details;
                details.push_back("field", std::string(key));
                err = ApiError::validationError(std::string(key) + " must be a decimal string",
                                                details);
                return false;
            }
            target = value;
            return true;
        };
        if (!checkMoney("price", patch.price) || !checkMoney("market_price", patch.market_price))
        {
            api::send(req, resp, err);
            return;
        }

        auto checkInt = [&](const char *key, std::optional<int64_t> &target, bool nonneg) {
            int64_t v = 0;
            if (!body.has(key))
                return true;
            if (!api::readInt(body, key, v) || (nonneg && v < 0))
            {
                wfrest::Json::Object details;
                details.push_back("field", std::string(key));
                err = ApiError::validationError(std::string(key) + " must be an integer",
                                                details);
                return false;
            }
            target = v;
            return true;
        };
        if (!checkInt("stock", patch.stock, true) || !checkInt("cat_id", patch.cat_id, true) ||
            !checkInt("brand_id", patch.brand_id, true))
        {
            api::send(req, resp, err);
            return;
        }

        auto checkBool = [&](const char *key, std::optional<bool> &target) -> bool {
            bool v = false;
            if (!body.has(key))
                return true;
            if (!api::readBool(body, key, v))
            {
                wfrest::Json::Object details;
                details.push_back("field", std::string(key));
                err = ApiError::validationError(std::string(key) + " must be a boolean", details);
                return false;
            }
            target = v;
            return true;
        };
        if (!checkBool("is_on_sale", patch.is_on_sale) || !checkBool("is_best", patch.is_best) ||
            !checkBool("is_new", patch.is_new) || !checkBool("is_hot", patch.is_hot) ||
            !checkBool("is_promote", patch.is_promote))
        {
            api::send(req, resp, err);
            return;
        }

        if (!goods->patch(goods_id, patch))
        {
            api::send(req, resp, ApiError::notFound("goods not found"));
            return;
        }

        std::optional<domain::AdminGoodsRow> updated = goods->find(goods_id);
        if (!updated)
        {
            api::send(req, resp, ApiError::internalError("goods patch failed"));
            return;
        }

        admins->writeLog(admin->admin_id, "patch_goods", "goods_id=" + std::to_string(goods_id),
                         task_of(resp)->peer_addr());
        api::send(req, resp, ApiResponse::ok(adminGoodsToJson(*updated)));
    });

    // DELETE /api/v1/admin/goods/{id} — soft delete (is_delete = 1)
    sv.DELETE("/api/v1/admin/goods/{id}",
              [admins, goods, db](const wfrest::HttpReq *req, wfrest::HttpResp *resp)
    {
        api::ApiResponse err;
        std::optional<domain::AdminUser> admin = adminAuth(req, admins, err);
        if (!admin)
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

        if (!goods->softDelete(goods_id))
        {
            api::send(req, resp, ApiError::notFound("goods not found"));
            return;
        }

        admins->writeLog(admin->admin_id, "delete_goods",
                         "goods_id=" + std::to_string(goods_id),
                         task_of(resp)->peer_addr());
        api::send(req, resp, ApiResponse::noContent());
    });
}

} // namespace ecshop::http
