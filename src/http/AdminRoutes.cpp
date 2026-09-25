#include "ecshop/http/AdminRoutes.h"
#include "ecshop/http/HttpUtil.h"
#include "ecshop/infrastructure/SqlAdminCategoryRepository.h"
#include "ecshop/infrastructure/SqlAdminGoodsRepository.h"
#include "ecshop/infrastructure/SqlAdminModerationRepository.h"
#include "ecshop/infrastructure/SqlAdminOrderRepository.h"
#include "ecshop/infrastructure/SqlAdminPaymentRepository.h"
#include "ecshop/infrastructure/SqlAdminRepository.h"
#include "ecshop/infrastructure/SqlAdminExtraRepository.h"
#include "ecshop/shared/Money.h"
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

// role gate: only super admins may touch users, funds and promotions
bool requireSuper(const std::optional<domain::AdminUser> &admin, ApiResponse &err)
{
    if (admin && admin->role == "super")
        return true;
    err = ApiError::forbidden("super role required for this operation");
    return false;
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

    auto admin_orders = std::make_shared<infra::SqlAdminOrderRepository>(db);

    // GET /api/v1/admin/orders — all orders
    sv.GET("/api/v1/admin/orders",
           [admins, admin_orders](const wfrest::HttpReq *req, wfrest::HttpResp *resp)
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

        domain::OrderPage result = admin_orders->list(page.offset(), page.page_size);

        wfrest::Json::Array items;
        for (const domain::OrderSummary &order : result.items)
        {
            wfrest::Json::Object obj;
            obj.push_back("id", order.order_id);
            obj.push_back("order_sn", order.order_sn);
            obj.push_back("status", order.status);
            obj.push_back("goods_amount", order.goods_amount);
            obj.push_back("shipping_fee", order.shipping_fee);
            obj.push_back("payment_fee", order.payment_fee);
            obj.push_back("order_amount", order.order_amount);
            obj.push_back("created_at", shared::isoUtc(
                                            std::strtoll(order.created_at.c_str(), nullptr, 10)));
            items.push_back(obj);
        }

        api::send(req, resp, ApiResponse::ok(api::listBody(page, result.total, items)));
    });

    // GET /api/v1/admin/orders/{id} — full order detail
    sv.GET("/api/v1/admin/orders/{id}",
           [admins, admin_orders](const wfrest::HttpReq *req, wfrest::HttpResp *resp)
    {
        api::ApiResponse err;
        std::optional<domain::AdminUser> admin = adminAuth(req, admins, err);
        if (!admin)
        {
            api::send(req, resp, err);
            return;
        }

        int64_t order_id = 0;
        if (!api::parsePathId(req, "id", order_id))
        {
            wfrest::Json::Object details;
            details.push_back("field", "id");
            api::send(req, resp, ApiError::validationError("id must be a positive integer", details));
            return;
        }

        std::optional<domain::OrderDetail> order = admin_orders->find(order_id);
        if (!order)
        {
            api::send(req, resp, ApiError::notFound("order not found"));
            return;
        }

        wfrest::Json::Object out;
        out.push_back("id", order->order_id);
        out.push_back("order_sn", order->order_sn);
        out.push_back("status", order->status);
        out.push_back("goods_amount", order->goods_amount);
        out.push_back("shipping_fee", order->shipping_fee);
        out.push_back("payment_fee", order->payment_fee);
        out.push_back("order_amount", order->order_amount);
        out.push_back("consignee", order->consignee);
        out.push_back("address", order->address);
        out.push_back("mobile", order->mobile);
        out.push_back("shipping_id", order->shipping_id);
        out.push_back("payment_id", order->payment_id);
        out.push_back("remark", order->remark);
        out.push_back("created_at",
                      shared::isoUtc(std::strtoll(order->created_at.c_str(), nullptr, 10)));

        wfrest::Json::Array items;
        for (const domain::OrderGoods &goods : order->items)
        {
            wfrest::Json::Object item;
            item.push_back("goods_id", goods.goods_id);
            item.push_back("name", goods.name);
            item.push_back("price", goods.price);
            item.push_back("quantity", goods.quantity);
            items.push_back(item);
        }
        out.push_back("items", items);
        api::send(req, resp, ApiResponse::ok(out));
    });

    // POST /api/v1/admin/orders/{id}/shipping — paid -> shipped
    sv.POST("/api/v1/admin/orders/{id}/shipping",
            [admins, admin_orders](const wfrest::HttpReq *req, wfrest::HttpResp *resp)
    {
        api::ApiResponse err;
        std::optional<domain::AdminUser> admin = adminAuth(req, admins, err);
        if (!admin)
        {
            api::send(req, resp, err);
            return;
        }

        int64_t order_id = 0;
        if (!api::parsePathId(req, "id", order_id))
        {
            wfrest::Json::Object details;
            details.push_back("field", "id");
            api::send(req, resp, ApiError::validationError("id must be a positive integer", details));
            return;
        }

        domain::OrderPatchStatus status = admin_orders->ship(order_id);
        switch (status)
        {
        case domain::OrderPatchStatus::Ok:
        {
            wfrest::Json::Object out;
            out.push_back("id", order_id);
            out.push_back("status", "shipped");
            api::send(req, resp, ApiResponse::ok(out));
            return;
        }
        case domain::OrderPatchStatus::InvalidState:
            api::send(req, resp, ApiError::invalidState("only paid orders can be shipped"));
            return;
        case domain::OrderPatchStatus::NotFound:
        default:
            api::send(req, resp, ApiError::notFound("order not found"));
            return;
        }
    });

    auto categories = std::make_shared<infra::SqlAdminCategoryRepository>(db);

    auto categoryToJson = [](const domain::CategorySummary &item) {
        wfrest::Json::Object obj;
        obj.push_back("cat_id", item.cat_id);
        obj.push_back("parent_id", item.parent_id);
        obj.push_back("name", item.name);
        obj.push_back("goods_count", item.goods_count);
        return obj;
    };

    // GET /api/v1/admin/categories — all categories incl. hidden
    sv.GET("/api/v1/admin/categories",
           [admins, categories, categoryToJson](const wfrest::HttpReq *req, wfrest::HttpResp *resp)
    {
        api::ApiResponse err;
        std::optional<domain::AdminUser> admin = adminAuth(req, admins, err);
        if (!admin)
        {
            api::send(req, resp, err);
            return;
        }

        std::vector<domain::CategorySummary> all = categories->list();

        wfrest::Json::Array items;
        for (const domain::CategorySummary &category : all)
            items.push_back(categoryToJson(category));

        api::PageQuery page;
        api::send(req, resp,
                  ApiResponse::ok(api::listBody(page, static_cast<int64_t>(all.size()), items)));
    });

    // POST /api/v1/admin/categories — create; 201
    sv.POST("/api/v1/admin/categories",
            [admins, categories, categoryToJson, db](const wfrest::HttpReq *req,
                                                     wfrest::HttpResp *resp)
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

        std::string name;
        if (!api::readStr(body, "name", name) || name.empty() || name.size() > 120)
        {
            wfrest::Json::Object details;
            details.push_back("field", "name");
            api::send(req, resp, ApiError::validationError("name must be 1-120 bytes", details));
            return;
        }
        int64_t parent_id = 0, sort_order = 50;
        if (body.has("parent_id") &&
            (!api::readInt(body, "parent_id", parent_id) || parent_id < 0))
        {
            wfrest::Json::Object details;
            details.push_back("field", "parent_id");
            api::send(req, resp,
                      ApiError::validationError("parent_id must be an integer", details));
            return;
        }
        if (body.has("sort_order") &&
            (!api::readInt(body, "sort_order", sort_order) || sort_order < 0))
        {
            wfrest::Json::Object details;
            details.push_back("field", "sort_order");
            api::send(req, resp,
                      ApiError::validationError("sort_order must be an integer", details));
            return;
        }

        int64_t cat_id = categories->create(name, parent_id, sort_order);
        domain::CategorySummary item;
        item.cat_id = cat_id;
        item.parent_id = parent_id;
        item.name = name;
        admins->writeLog(admin->admin_id, "create_category", "cat_id=" + std::to_string(cat_id),
                         task_of(resp)->peer_addr());
        api::send(req, resp, ApiResponse::created(categoryToJson(item)));
    });

    // PATCH /api/v1/admin/categories/{id}
    sv.PATCH("/api/v1/admin/categories/{id}",
             [admins, categories, categoryToJson, db](const wfrest::HttpReq *req,
                                                      wfrest::HttpResp *resp)
    {
        api::ApiResponse err;
        std::optional<domain::AdminUser> admin = adminAuth(req, admins, err);
        if (!admin)
        {
            api::send(req, resp, err);
            return;
        }

        int64_t cat_id = 0;
        if (!api::parsePathId(req, "id", cat_id))
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

        std::optional<std::string> name;
        std::optional<int64_t> parent_id, sort_order;
        std::optional<bool> is_show;

        if (body.has("name"))
        {
            std::string value;
            if (!api::readStr(body, "name", value) || value.empty() || value.size() > 120)
            {
                wfrest::Json::Object details;
                details.push_back("field", "name");
                api::send(req, resp,
                          ApiError::validationError("name must be 1-120 bytes", details));
                return;
            }
            name = value;
        }
        auto checkInt = [&](const char *key, std::optional<int64_t> &target) -> bool {
            int64_t v = 0;
            if (!body.has(key))
                return true;
            if (!api::readInt(body, key, v) || v < 0)
            {
                wfrest::Json::Object details;
                details.push_back("field", std::string(key));
                api::send(req, resp,
                          ApiError::validationError(std::string(key) + " must be an integer",
                                                    details));
                return false;
            }
            target = v;
            return true;
        };
        if (!checkInt("parent_id", parent_id) || !checkInt("sort_order", sort_order))
            return;

        if (body.has("is_show"))
        {
            bool value = false;
            if (!api::readBool(body, "is_show", value))
            {
                wfrest::Json::Object details;
                details.push_back("field", "is_show");
                api::send(req, resp,
                          ApiError::validationError("is_show must be a boolean", details));
                return;
            }
            is_show = value;
        }

        if (!categories->patch(cat_id, name, parent_id, sort_order, is_show))
        {
            api::send(req, resp, ApiError::notFound("category not found"));
            return;
        }

        admins->writeLog(admin->admin_id, "patch_category", "cat_id=" + std::to_string(cat_id),
                         task_of(resp)->peer_addr());

        // echo back the full category list row re-fetched
        std::vector<domain::CategorySummary> all = categories->list();
        for (const domain::CategorySummary &category : all)
        {
            if (category.cat_id == cat_id)
            {
                api::send(req, resp, ApiResponse::ok(categoryToJson(category)));
                return;
            }
        }
        api::send(req, resp, ApiError::internalError("category patch failed"));
    });

    // DELETE /api/v1/admin/categories/{id}
    sv.DELETE("/api/v1/admin/categories/{id}",
              [admins, categories, db](const wfrest::HttpReq *req, wfrest::HttpResp *resp)
    {
        api::ApiResponse err;
        std::optional<domain::AdminUser> admin = adminAuth(req, admins, err);
        if (!admin)
        {
            api::send(req, resp, err);
            return;
        }

        int64_t cat_id = 0;
        if (!api::parsePathId(req, "id", cat_id))
        {
            wfrest::Json::Object details;
            details.push_back("field", "id");
            api::send(req, resp, ApiError::validationError("id must be a positive integer", details));
            return;
        }

        if (!categories->remove(cat_id))
        {
            api::send(req, resp,
                      ApiError::conflict("category_not_empty",
                                         "category in use by goods or not found"));
            return;
        }

        admins->writeLog(admin->admin_id, "delete_category", "cat_id=" + std::to_string(cat_id),
                         task_of(resp)->peer_addr());
        api::send(req, resp, ApiResponse::noContent());
    });

    auto payments = std::make_shared<infra::SqlAdminPaymentRepository>(db);
    auto shippings = std::make_shared<infra::SqlAdminShippingRepository>(db);

    auto paymentToJson = [](const domain::PaymentOption &item) {
        wfrest::Json::Object obj;
        obj.push_back("payment_id", item.payment_id);
        obj.push_back("name", item.name);
        obj.push_back("fee", item.fee);
        return obj;
    };
    auto shippingToJson = [](const domain::ShippingOption &item) {
        wfrest::Json::Object obj;
        obj.push_back("shipping_id", item.shipping_id);
        obj.push_back("name", item.name);
        obj.push_back("fee", item.fee);
        return obj;
    };

    // ---- payment methods ----

    sv.GET("/api/v1/admin/payments",
           [admins, payments, db, paymentToJson](const wfrest::HttpReq *req,
                                                  wfrest::HttpResp *resp)
    {
        api::ApiResponse err;
        std::optional<domain::AdminUser> admin = adminAuth(req, admins, err);
        if (!admin)
        {
            api::send(req, resp, err);
            return;
        }
        std::vector<domain::PaymentOption> all = payments->listAll();
        wfrest::Json::Array items;
        for (const domain::PaymentOption &item : all)
            items.push_back(paymentToJson(item));
        api::PageQuery page;
        api::send(req, resp,
                  ApiResponse::ok(api::listBody(page, static_cast<int64_t>(all.size()), items)));
    });

    sv.POST("/api/v1/admin/payments",
            [admins, payments, db, paymentToJson](const wfrest::HttpReq *req,
                                                   wfrest::HttpResp *resp)
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
        std::string name, fee;
        if (!api::readStr(body, "name", name) || name.empty() || name.size() > 120)
        {
            wfrest::Json::Object details;
            details.push_back("field", "name");
            api::send(req, resp, ApiError::validationError("name must be 1-120 bytes", details));
            return;
        }
        if (!api::readStr(body, "fee", fee))
            fee = "0.00";
        bool enabled = true;
        if (body.has("enabled") && !api::readBool(body, "enabled", enabled))
        {
            wfrest::Json::Object details;
            details.push_back("field", "enabled");
            api::send(req, resp, ApiError::validationError("enabled must be a boolean", details));
            return;
        }
        payments->create(name, fee, enabled);
        domain::PaymentOption item;
        item.payment_id = db->lastInsertId();
        item.name = name;
        item.fee = shared::Money::normalize(fee);
        admins->writeLog(admin->admin_id, "create_payment", "pay_id=" + std::to_string(item.payment_id),
                         task_of(resp)->peer_addr());
        api::send(req, resp, ApiResponse::ok(paymentToJson(item)));
    });

    sv.PATCH("/api/v1/admin/payments/{id}",
             [admins, payments, db, paymentToJson](const wfrest::HttpReq *req,
                                                    wfrest::HttpResp *resp)
    {
        api::ApiResponse err;
        std::optional<domain::AdminUser> admin = adminAuth(req, admins, err);
        if (!admin)
        {
            api::send(req, resp, err);
            return;
        }
        int64_t pay_id = 0;
        if (!api::parsePathId(req, "id", pay_id))
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
        std::optional<std::string> name, fee;
        std::optional<bool> enabled;
        std::string value;
        if (body.has("name"))
        {
            if (!api::readStr(body, "name", value) || value.empty() || value.size() > 120)
            {
                wfrest::Json::Object details;
                details.push_back("field", "name");
                api::send(req, resp,
                          ApiError::validationError("name must be 1-120 bytes", details));
                return;
            }
            name = value;
        }
        if (body.has("fee"))
        {
            if (!api::readStr(body, "fee", value))
            {
                wfrest::Json::Object details;
                details.push_back("field", "fee");
                api::send(req, resp,
                          ApiError::validationError("fee must be a decimal string", details));
                return;
            }
            fee = value;
        }
        if (body.has("enabled"))
        {
            bool v = false;
            if (!api::readBool(body, "enabled", v))
            {
                wfrest::Json::Object details;
                details.push_back("field", "enabled");
                api::send(req, resp,
                          ApiError::validationError("enabled must be a boolean", details));
                return;
            }
            enabled = v;
        }
        if (!payments->patch(pay_id, name, fee, enabled))
        {
            api::send(req, resp, ApiError::notFound("payment method not found"));
            return;
        }
        std::optional<domain::PaymentOption> item = payments->find(pay_id);
        if (!item)
        {
            api::send(req, resp, ApiError::notFound("payment method not found"));
            return;
        }
        admins->writeLog(admin->admin_id, "patch_payment", "pay_id=" + std::to_string(pay_id),
                         task_of(resp)->peer_addr());
        api::send(req, resp, ApiResponse::ok(paymentToJson(*item)));
    });

    // ---- shipping methods ----

    sv.GET("/api/v1/admin/shippings",
           [admins, shippings, db, shippingToJson](const wfrest::HttpReq *req,
                                                    wfrest::HttpResp *resp)
    {
        api::ApiResponse err;
        std::optional<domain::AdminUser> admin = adminAuth(req, admins, err);
        if (!admin)
        {
            api::send(req, resp, err);
            return;
        }
        std::vector<domain::ShippingOption> all = shippings->listAll();
        wfrest::Json::Array items;
        for (const domain::ShippingOption &item : all)
            items.push_back(shippingToJson(item));
        api::PageQuery page;
        api::send(req, resp,
                  ApiResponse::ok(api::listBody(page, static_cast<int64_t>(all.size()), items)));
    });

    sv.POST("/api/v1/admin/shippings",
            [admins, shippings, db, shippingToJson](const wfrest::HttpReq *req,
                                                     wfrest::HttpResp *resp)
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
        std::string name, fee;
        if (!api::readStr(body, "name", name) || name.empty() || name.size() > 120)
        {
            wfrest::Json::Object details;
            details.push_back("field", "name");
            api::send(req, resp, ApiError::validationError("name must be 1-120 bytes", details));
            return;
        }
        if (!api::readStr(body, "fee", fee))
            fee = "0.00";
        bool enabled = true;
        if (body.has("enabled") && !api::readBool(body, "enabled", enabled))
        {
            wfrest::Json::Object details;
            details.push_back("field", "enabled");
            api::send(req, resp, ApiError::validationError("enabled must be a boolean", details));
            return;
        }
        shippings->create(name, fee, enabled);
        domain::ShippingOption item;
        item.shipping_id = db->lastInsertId();
        item.name = name;
        item.fee = shared::Money::normalize(fee);
        admins->writeLog(admin->admin_id, "create_shipping",
                         "shipping_id=" + std::to_string(item.shipping_id),
                         task_of(resp)->peer_addr());
        api::send(req, resp, ApiResponse::ok(shippingToJson(item)));
    });

    sv.PATCH("/api/v1/admin/shippings/{id}",
             [admins, shippings, db, shippingToJson](const wfrest::HttpReq *req,
                                                      wfrest::HttpResp *resp)
    {
        api::ApiResponse err;
        std::optional<domain::AdminUser> admin = adminAuth(req, admins, err);
        if (!admin)
        {
            api::send(req, resp, err);
            return;
        }
        int64_t shipping_id = 0;
        if (!api::parsePathId(req, "id", shipping_id))
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
        std::optional<std::string> name, fee;
        std::optional<bool> enabled;
        std::string value;
        if (body.has("name"))
        {
            if (!api::readStr(body, "name", value) || value.empty() || value.size() > 120)
            {
                wfrest::Json::Object details;
                details.push_back("field", "name");
                api::send(req, resp,
                          ApiError::validationError("name must be 1-120 bytes", details));
                return;
            }
            name = value;
        }
        if (body.has("fee"))
        {
            if (!api::readStr(body, "fee", value))
            {
                wfrest::Json::Object details;
                details.push_back("field", "fee");
                api::send(req, resp,
                          ApiError::validationError("fee must be a decimal string", details));
                return;
            }
            fee = value;
        }
        if (body.has("enabled"))
        {
            bool v = false;
            if (!api::readBool(body, "enabled", v))
            {
                wfrest::Json::Object details;
                details.push_back("field", "enabled");
                api::send(req, resp,
                          ApiError::validationError("enabled must be a boolean", details));
                return;
            }
            enabled = v;
        }
        if (!shippings->patch(shipping_id, name, fee, enabled))
        {
            api::send(req, resp, ApiError::notFound("shipping method not found"));
            return;
        }
        std::optional<domain::ShippingOption> item = shippings->find(shipping_id);
        if (!item)
        {
            api::send(req, resp, ApiError::notFound("shipping method not found"));
            return;
        }
        admins->writeLog(admin->admin_id, "patch_shipping",
                         "shipping_id=" + std::to_string(shipping_id),
                         task_of(resp)->peer_addr());
        api::send(req, resp, ApiResponse::ok(shippingToJson(*item)));
    });

    auto moderation = std::make_shared<infra::SqlAdminModerationRepository>(db);

    auto adminCommentToJson = [](const domain::Comment &comment) {
        wfrest::Json::Object obj;
        obj.push_back("comment_id", comment.comment_id);
        obj.push_back("goods_id", comment.goods_id);
        obj.push_back("user_name", comment.user_name);
        obj.push_back("content", comment.content);
        obj.push_back("add_time",
                      shared::isoUtc(std::strtoll(comment.add_time.c_str(), nullptr, 10)));
        return obj;
    };

    // GET /api/v1/admin/comments — all comments incl. unpublished
    sv.GET("/api/v1/admin/comments",
           [admins, moderation, adminCommentToJson](const wfrest::HttpReq *req,
                                                     wfrest::HttpResp *resp)
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
        std::vector<domain::Comment> all =
            moderation->listComments(page.offset(), page.page_size);
        wfrest::Json::Array items;
        for (const domain::Comment &comment : all)
            items.push_back(adminCommentToJson(comment));
        api::send(req, resp,
                  ApiResponse::ok(api::listBody(page, moderation->countComments(), items)));
    });

    // PATCH /api/v1/admin/comments/{id}/status — {"published":true|false}
    sv.PATCH("/api/v1/admin/comments/{id}/status",
             [admins, moderation, db](const wfrest::HttpReq *req, wfrest::HttpResp *resp)
    {
        api::ApiResponse err;
        std::optional<domain::AdminUser> admin = adminAuth(req, admins, err);
        if (!admin)
        {
            api::send(req, resp, err);
            return;
        }
        int64_t comment_id = 0;
        if (!api::parsePathId(req, "id", comment_id))
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
        bool published = false;
        if (!api::readBool(body, "published", published))
        {
            wfrest::Json::Object details;
            details.push_back("field", "published");
            api::send(req, resp, ApiError::validationError("published must be a boolean", details));
            return;
        }
        if (!moderation->setCommentStatus(comment_id, published))
        {
            api::send(req, resp, ApiError::notFound("comment not found"));
            return;
        }
        admins->writeLog(admin->admin_id, "comment_status",
                         "comment_id=" + std::to_string(comment_id),
                         task_of(resp)->peer_addr());
        api::send(req, resp, ApiResponse::noContent());
    });

    // DELETE /api/v1/admin/comments/{id}
    sv.DELETE("/api/v1/admin/comments/{id}",
              [admins, moderation, db](const wfrest::HttpReq *req, wfrest::HttpResp *resp)
    {
        api::ApiResponse err;
        std::optional<domain::AdminUser> admin = adminAuth(req, admins, err);
        if (!admin)
        {
            api::send(req, resp, err);
            return;
        }
        int64_t comment_id = 0;
        if (!api::parsePathId(req, "id", comment_id))
        {
            wfrest::Json::Object details;
            details.push_back("field", "id");
            api::send(req, resp, ApiError::validationError("id must be a positive integer", details));
            return;
        }
        if (!moderation->removeComment(comment_id))
        {
            api::send(req, resp, ApiError::notFound("comment not found"));
            return;
        }
        admins->writeLog(admin->admin_id, "delete_comment",
                         "comment_id=" + std::to_string(comment_id),
                         task_of(resp)->peer_addr());
        api::send(req, resp, ApiResponse::noContent());
    });

    auto adminMessageToJson = [](const domain::Message &message) {
        wfrest::Json::Object obj;
        obj.push_back("msg_id", message.msg_id);
        obj.push_back("username", message.username);
        obj.push_back("email", message.email);
        obj.push_back("title", message.title);
        obj.push_back("content", message.content);
        obj.push_back("order_id", message.order_id);
        obj.push_back("published", message.published);
        obj.push_back("created_at",
                      shared::isoUtc(std::strtoll(message.created_at.c_str(), nullptr, 10)));
        return obj;
    };

    // GET /api/v1/admin/messages — all board messages incl. unpublished
    sv.GET("/api/v1/admin/messages",
           [admins, moderation, adminMessageToJson](const wfrest::HttpReq *req,
                                                     wfrest::HttpResp *resp)
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
        std::vector<domain::Message> all = moderation->listMessages(page.offset(), page.page_size);
        wfrest::Json::Array items;
        for (const domain::Message &message : all)
            items.push_back(adminMessageToJson(message));
        api::send(req, resp,
                  ApiResponse::ok(api::listBody(page, moderation->countMessages(), items)));
    });

    // PATCH /api/v1/admin/messages/{id}/status — {"published":true|false}
    sv.PATCH("/api/v1/admin/messages/{id}/status",
             [admins, moderation, db](const wfrest::HttpReq *req, wfrest::HttpResp *resp)
    {
        api::ApiResponse err;
        std::optional<domain::AdminUser> admin = adminAuth(req, admins, err);
        if (!admin)
        {
            api::send(req, resp, err);
            return;
        }
        int64_t msg_id = 0;
        if (!api::parsePathId(req, "id", msg_id))
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
        bool published = false;
        if (!api::readBool(body, "published", published))
        {
            wfrest::Json::Object details;
            details.push_back("field", "published");
            api::send(req, resp, ApiError::validationError("published must be a boolean", details));
            return;
        }
        if (!moderation->setMessageStatus(msg_id, published))
        {
            api::send(req, resp, ApiError::notFound("message not found"));
            return;
        }
        admins->writeLog(admin->admin_id, "message_status",
                         "msg_id=" + std::to_string(msg_id),
                         task_of(resp)->peer_addr());
        api::send(req, resp, ApiResponse::noContent());
    });

    // DELETE /api/v1/admin/messages/{id} — removes replies too
    sv.DELETE("/api/v1/admin/messages/{id}",
              [admins, moderation, db](const wfrest::HttpReq *req, wfrest::HttpResp *resp)
    {
        api::ApiResponse err;
        std::optional<domain::AdminUser> admin = adminAuth(req, admins, err);
        if (!admin)
        {
            api::send(req, resp, err);
            return;
        }
        int64_t msg_id = 0;
        if (!api::parsePathId(req, "id", msg_id))
        {
            wfrest::Json::Object details;
            details.push_back("field", "id");
            api::send(req, resp, ApiError::validationError("id must be a positive integer", details));
            return;
        }
        if (!moderation->removeMessage(msg_id))
        {
            api::send(req, resp, ApiError::notFound("message not found"));
            return;
        }
        admins->writeLog(admin->admin_id, "delete_message",
                         "msg_id=" + std::to_string(msg_id),
                         task_of(resp)->peer_addr());
        api::send(req, resp, ApiResponse::noContent());
    });

    auto extra = std::make_shared<infra::SqlAdminExtraRepository>(db);

    // GET /api/v1/admin/users — front-end user accounts
    sv.GET("/api/v1/admin/users",
           [admins, extra](const wfrest::HttpReq *req, wfrest::HttpResp *resp)
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
        auto result = extra->listUsers(page.offset(), page.page_size);
        wfrest::Json::Array items;
        for (const auto &user : result.items)
        {
            wfrest::Json::Object obj;
            obj.push_back("user_id", user.user_id);
            obj.push_back("username", user.username);
            obj.push_back("email", user.email);
            obj.push_back("created_at",
                          shared::isoUtc(std::strtoll(user.created_at.c_str(), nullptr, 10)));
            items.push_back(obj);
        }
        api::send(req, resp, api::ApiResponse::ok(api::listBody(page, result.total, items)));
    });

    // PATCH /api/v1/admin/users/{id} — {"email":"..."}
    sv.PATCH("/api/v1/admin/users/{id}",
             [admins, extra](const wfrest::HttpReq *req, wfrest::HttpResp *resp)
    {
        api::ApiResponse err;
        std::optional<domain::AdminUser> admin = adminAuth(req,admins, err);
        if (!admin)
        {
            api::send(req, resp, err);
            return;
        }
        int64_t user_id = 0;
        if (!api::parsePathId(req, "id", user_id))
        {
            wfrest::Json::Object details;
            details.push_back("field", "id");
            api::send(req, resp,
                      ApiError::validationError("id must be a positive integer", details));
            return;
        }
        wfrest::Json body;
        if (!api::parseJsonBody(req, body, err))
        {
            api::send(req, resp, err);
            return;
        }
        std::string email;
        if (!api::readStr(body, "email", email) || email.find('@') == std::string::npos)
        {
            wfrest::Json::Object details;
            details.push_back("field", "email");
            api::send(req, resp,
                      ApiError::validationError("email must be a valid address", details));
            return;
        }
        if (!requireSuper(admin, err))
        {
            api::send(req, resp, err);
            return;
        }
        if (!extra->updateUserEmail(user_id, email))
        {
            api::send(req, resp, ApiError::notFound("user not found"));
            return;
        }
        admins->writeLog(admin->admin_id, "patch_user", "user_id=" + std::to_string(user_id),
                         task_of(resp)->peer_addr());
        wfrest::Json::Object out;
        out.push_back("user_id", user_id);
        out.push_back("email", email);
        api::send(req, resp, api::ApiResponse::ok(out));
    });

    // POST /api/v1/admin/promotions — create goods_activity row (0..2 types)
    sv.POST("/api/v1/admin/promotions",
            [admins, extra, db](const wfrest::HttpReq *req, wfrest::HttpResp *resp)
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
        std::string name;
        if (!api::readStr(body, "name", name) || name.empty() || name.size() > 255)
        {
            wfrest::Json::Object details;
            details.push_back("field", "name");
            api::send(req, resp, ApiError::validationError("name must be 1-255 bytes", details));
            return;
        }
        int64_t act_type = 0, goods_id = 0;
        // 0=snatch 1=group_buy 2=auction; exchange/package have dedicated flows
        if (!api::readInt(body, "act_type", act_type) || act_type < 0 || act_type > 2)
        {
            wfrest::Json::Object details;
            details.push_back("field", "act_type");
            api::send(req, resp, ApiError::validationError("act_type must be 0-2", details));
            return;
        }
        if (!api::readInt(body, "goods_id", goods_id) || goods_id <= 0)
        {
            wfrest::Json::Object details;
            details.push_back("field", "goods_id");
            api::send(req, resp, ApiError::validationError("goods_id is required", details));
            return;
        }
        int64_t start_time = 0, end_time = 0;
        if (!api::readInt(body, "start_time", start_time) || start_time < 0)
        {
            wfrest::Json::Object details;
            details.push_back("field", "start_time");
            api::send(req, resp, ApiError::validationError("start_time is required", details));
            return;
        }
        if (!api::readInt(body, "end_time", end_time) || end_time < start_time)
        {
            wfrest::Json::Object details;
            details.push_back("field", "end_time");
            api::send(req, resp, ApiError::validationError("end_time must be >= start_time",
                                                            details));
            return;
        }
        std::string description, ext_info;
        api::readStr(body, "description", description);
        api::readStr(body, "ext_info", ext_info);

        if (!requireSuper(admin, err))
        {
            api::send(req, resp, err);
            return;
        }
        int64_t act_id = 0;
        try
        {
            act_id = extra->createPromotion(name, description, act_type, goods_id, start_time,
                                            end_time, ext_info);
        }
        catch (const infra::DbError &)
        {
            api::send(req, resp, ApiError::conflict("promotion_conflict", "promotion rejected"));
            return;
        }

        admins->writeLog(admin->admin_id, "create_promotion",
                         "act_id=" + std::to_string(act_id),
                         task_of(resp)->peer_addr());
        wfrest::Json::Object out;
        out.push_back("act_id", act_id);
        api::send(req, resp, api::ApiResponse::created(out));
    });

    // DELETE /api/v1/admin/promotions/{id}?act_type=N — remove by id (type bound)
    sv.DELETE("/api/v1/admin/promotions/{id}",
              [admins, extra, db](const wfrest::HttpReq *req, wfrest::HttpResp *resp)
    {
        api::ApiResponse err;
        std::optional<domain::AdminUser> admin = adminAuth(req, admins, err);
        if (!admin)
        {
            api::send(req, resp, err);
            return;
        }
        int64_t act_id = 0;
        if (!api::parsePathId(req, "id", act_id))
        {
            wfrest::Json::Object details;
            details.push_back("field", "id");
            api::send(req, resp,
                      ApiError::validationError("id must be a positive integer", details));
            return;
        }
        int64_t act_type = 0;
        const std::string &raw_type = req->query("act_type");
        if (!raw_type.empty())
        {
            char *end = nullptr;
            long long v = std::strtoll(raw_type.c_str(), &end, 10);
            if (!end || *end != '\0' || v < 0 || v > 4)
            {
                wfrest::Json::Object details;
                details.push_back("field", "act_type");
                api::send(req, resp, ApiError::validationError("act_type must be 0-4", details));
                return;
            }
            act_type = v;
        }
        if (!requireSuper(admin, err))
        {
            api::send(req, resp, err);
            return;
        }
        if (!extra->deletePromotion(act_id, act_type))
        {
            api::send(req, resp, ApiError::notFound("promotion not found"));
            return;
        }
        admins->writeLog(admin->admin_id, "delete_promotion",
                         "act_id=" + std::to_string(act_id),
                         task_of(resp)->peer_addr());
        api::send(req, resp, api::ApiResponse::noContent());
    });

    // POST /api/v1/admin/account/requests/{id}/settle — deposit/withdrawal finish
    sv.POST("/api/v1/admin/account/requests/{id}/settle",
            [admins, extra, db](const wfrest::HttpReq *req, wfrest::HttpResp *resp)
    {
        api::ApiResponse err;
        std::optional<domain::AdminUser> admin = adminAuth(req, admins, err);
        if (!admin)
        {
            api::send(req, resp, err);
            return;
        }
        int64_t rec_id = 0;
        if (!api::parsePathId(req, "id", rec_id))
        {
            wfrest::Json::Object details;
            details.push_back("field", "id");
            api::send(req, resp,
                      ApiError::validationError("id must be a positive integer", details));
            return;
        }
        std::optional<infra::SqlAdminExtraRepository::SettleRequest> request =
            extra->findAccountRequest(rec_id);
        if (!request)
        {
            api::send(req, resp, ApiError::notFound("request not found"));
            return;
        }

        bool ok = false;
        if (!requireSuper(admin, err))
        {
            api::send(req, resp, err);
            return;
        }
        if (request->process_type == "deposit")
            ok = extra->settleDeposit(rec_id, 0);
        else if (request->process_type == "withdrawal")
            ok = extra->settleWithdrawal(rec_id, 0);

        if (!ok)
        {
            api::send(req, resp,
                      ApiError::conflict("settle_conflict",
                                         "request not in a settleable state"));
            return;
        }

        std::string new_status = request->process_type == "deposit" ? "paid" : "disbursed";
        admins->writeLog(admin->admin_id, "settle_request",
                         "rec_id=" + std::to_string(rec_id) + " status=" + new_status,
                         task_of(resp)->peer_addr());
        wfrest::Json::Object out;
        out.push_back("id", rec_id);
        out.push_back("kind", request->process_type);
        out.push_back("amount", shared::Money::format(request->amount_cents));
        out.push_back("status", new_status);
        api::send(req, resp, api::ApiResponse::ok(out));
    });

    // GET /api/v1/admin/logs — audit trail
    sv.GET("/api/v1/admin/logs",
           [admins](const wfrest::HttpReq *req, wfrest::HttpResp *resp)
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
        std::vector<domain::AdminLogRow> all = admins->listLogs(page.offset(), page.page_size);
        wfrest::Json::Array items;
        for (const domain::AdminLogRow &row : all)
        {
            wfrest::Json::Object obj;
            obj.push_back("log_id", row.log_id);
            obj.push_back("admin_id", row.admin_id);
            obj.push_back("admin_name", row.admin_name);
            obj.push_back("action", row.action);
            obj.push_back("detail", row.detail);
            obj.push_back("ip_address", row.ip_address);
            obj.push_back("created_at",
                          shared::isoUtc(std::strtoll(row.created_at.c_str(), nullptr, 10)));
            items.push_back(obj);
        }
        // total is capped at the visible number; broad counting is unnecessary
        api::send(req, resp, api::ApiResponse::ok(
                                 api::listBody(page, static_cast<int64_t>(items.size()), items)));
    });

    // GET /api/v1/admin/metrics — rough counters
    sv.GET("/api/v1/admin/metrics",
           [admins, extra](const wfrest::HttpReq *req, wfrest::HttpResp *resp)
    {
        api::ApiResponse err;
        std::optional<domain::AdminUser> admin = adminAuth(req, admins, err);
        if (!admin)
        {
            api::send(req, resp, err);
            return;
        }
        wfrest::Json::Object out;
        out.push_back("users", extra->countUsers());
        out.push_back("orders", extra->countOrders());
        out.push_back("goods", extra->countGoodsAll());
        out.push_back("paid_orders", extra->countRevenues());
        api::send(req, resp, api::ApiResponse::ok(out));
    });

    // ---- admin account management (super only) ----

    // GET /api/v1/admin/admins
    sv.GET("/api/v1/admin/admins",
           [admins](const wfrest::HttpReq *req, wfrest::HttpResp *resp)
    {
        api::ApiResponse err;
        std::optional<domain::AdminUser> admin = adminAuth(req, admins, err);
        if (!admin)
        {
            api::send(req, resp, err);
            return;
        }
        if (!requireSuper(admin, err))
        {
            api::send(req, resp, err);
            return;
        }

        wfrest::Json::Array items;
        api::PageQuery page;
        for (const domain::AdminUser &one : admins->listAdmins())
        {
            wfrest::Json::Object obj;
            obj.push_back("admin_id", one.admin_id);
            obj.push_back("username", one.username);
            obj.push_back("role", one.role);
            items.push_back(obj);
        }
        api::send(req, resp,
                  api::ApiResponse::ok(api::listBody(
                      page, static_cast<int64_t>(items.size()), items)));
    });

    // POST /api/v1/admin/admins — {"username","password","role":"manager"}
    sv.POST("/api/v1/admin/admins",
            [admins, db](const wfrest::HttpReq *req, wfrest::HttpResp *resp)
    {
        api::ApiResponse err;
        std::optional<domain::AdminUser> admin = adminAuth(req, admins, err);
        if (!admin)
        {
            api::send(req, resp, err);
            return;
        }
        if (!requireSuper(admin, err))
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

        std::string username, password, role = "manager";
        if (!api::readStr(body, "username", username) || username.empty() ||
            username.size() > 60)
        {
            wfrest::Json::Object details;
            details.push_back("field", "username");
            api::send(req, resp,
                      ApiError::validationError("username must be 1-60 bytes", details));
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
        std::string role_input;
        if (body.has("role"))
        {
            if (!api::readStr(body, "role", role_input) ||
                (role_input != "super" && role_input != "manager"))
            {
                wfrest::Json::Object details;
                details.push_back("field", "role");
                api::send(req, resp,
                          ApiError::validationError("role must be super or manager", details));
                return;
            }
            role = role_input;
        }

        if (admins->findByUsername(username))
        {
            api::send(req, resp,
                      ApiError::conflict("username_taken", "admin username is taken"));
            return;
        }

        int64_t created_id = 0;
        try
        {
            created_id = admins->createAdmin(username, shared::hashPassword(password), role);
        }
        catch (const infra::DbError &)
        {
            api::send(req, resp,
                      ApiError::conflict("username_taken", "admin username is taken"));
            return;
        }

        admins->writeLog(admin->admin_id, "create_admin",
                         "admin_id=" + std::to_string(created_id) + " role=" + role,
                         task_of(resp)->peer_addr());

        wfrest::Json::Object out;
        out.push_back("admin_id", created_id);
        out.push_back("username", username);
        out.push_back("role", role);
        api::send(req, resp, api::ApiResponse::created(out));
    });

    // DELETE /api/v1/admin/admins/{id} — cannot delete self or the last super
    sv.DELETE("/api/v1/admin/admins/{id}",
              [admins, db](const wfrest::HttpReq *req, wfrest::HttpResp *resp)
    {
        api::ApiResponse err;
        std::optional<domain::AdminUser> admin = adminAuth(req, admins, err);
        if (!admin)
        {
            api::send(req, resp, err);
            return;
        }
        if (!requireSuper(admin, err))
        {
            api::send(req, resp, err);
            return;
        }
        int64_t target_id = 0;
        if (!api::parsePathId(req, "id", target_id))
        {
            wfrest::Json::Object details;
            details.push_back("field", "id");
            api::send(req, resp,
                      ApiError::validationError("id must be a positive integer", details));
            return;
        }
        if (target_id == admin->admin_id)
        {
            api::send(req, resp,
                      ApiError::conflict("admin_self_delete", "you cannot delete yourself"));
            return;
        }

        std::optional<domain::AdminUser> target = admins->findById(target_id);
        if (!target)
        {
            api::send(req, resp, ApiError::notFound("admin not found"));
            return;
        }
        if (target->role == "super")
        {
            int64_t supers = 0;
            for (const domain::AdminUser &one : admins->listAdmins())
            {
                if (one.role == "super")
                    ++supers;
            }
            if (supers <= 1)
            {
                api::send(req, resp,
                          ApiError::conflict("last_super_admin", "cannot delete the last super admin"));
                return;
            }
        }

        if (!admins->deleteAdmin(target_id))
        {
            api::send(req, resp, ApiError::notFound("admin not found"));
            return;
        }

        admins->writeLog(admin->admin_id, "delete_admin",
                         "admin_id=" + std::to_string(target_id),
                         task_of(resp)->peer_addr());
        api::send(req, resp, api::ApiResponse::noContent());
    });
}

} // namespace ecshop::http
