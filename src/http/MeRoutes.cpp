#include "ecshop/http/MeRoutes.h"
#include "ecshop/http/AuthUtil.h"
#include "ecshop/http/HttpUtil.h"
#include "ecshop/infrastructure/SqlAccountRepository.h"
#include "ecshop/infrastructure/SqlAccountTokenRepository.h"
#include "ecshop/infrastructure/SqlBonusRepository.h"
#include "ecshop/infrastructure/SqlUserRepository.h"
#include "ecshop/shared/Money.h"
#include "ecshop/shared/Password.h"
#include "ecshop/shared/TimeUtil.h"

#include <cstdlib>
#include <optional>
#include <string>
#include <vector>

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
    auto bonuses = std::make_shared<infra::SqlBonusRepository>(db);
    auto tokens = std::make_shared<infra::SqlAccountTokenRepository>(db);
    auto account = std::make_shared<infra::SqlAccountRepository>(db);

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

    // POST /api/v1/me/bonuses/claim — claim a bonus by serial number
    sv.POST("/api/v1/me/bonuses/claim",
            [users, bonuses, db](const wfrest::HttpReq *req, wfrest::HttpResp *resp)
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

        std::string bonus_sn;
        if (!api::readStr(body, "bonus_sn", bonus_sn) || bonus_sn.empty() ||
            bonus_sn.size() > 20 ||
            bonus_sn.find_first_not_of("0123456789") != std::string::npos)
        {
            wfrest::Json::Object details;
            details.push_back("field", "bonus_sn");
            api::send(req, resp,
                      ApiError::validationError("bonus_sn must be 1-20 digits", details));
            return;
        }

        std::optional<domain::BonusRecord> bonus = bonuses->findBySn(bonus_sn);
        if (!bonus)
        {
            api::send(req, resp, ApiError::notFound("bonus not found"));
            return;
        }
        if (bonus->user_id != 0)
        {
            api::send(req, resp, ApiError::conflict("bonus_claimed", "bonus already claimed"));
            return;
        }

        int64_t now = shared::nowUnix();
        if ((bonus->use_start_date > 0 && now < bonus->use_start_date) ||
            (bonus->use_end_date > 0 && now > bonus->use_end_date))
        {
            api::send(req, resp, ApiError::conflict("bonus_expired", "bonus is not usable now"));
            return;
        }

        bool claimed = false;
        try
        {
            db->transaction([&] {
                claimed = bonuses->claim(bonus->bonus_id, *user_id);
            });
        }
        catch (const infra::DbError &)
        {
            claimed = false;
        }

        if (!claimed)
        {
            api::send(req, resp, ApiError::conflict("bonus_claimed", "bonus already claimed"));
            return;
        }

        api::send(req, resp, ApiResponse::noContent());
    });

    // GET /api/v1/me/bonuses — bonuses claimed by the current user
    sv.GET("/api/v1/me/bonuses",
           [users, bonuses](const wfrest::HttpReq *req, wfrest::HttpResp *resp)
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

        std::vector<domain::BonusRecord> all = bonuses->listOfUser(*user_id);
        int64_t total = static_cast<int64_t>(all.size());

        int64_t now = shared::nowUnix();
        wfrest::Json::Array items;
        int64_t index = 0;
        for (const domain::BonusRecord &bonus : all)
        {
            if (index < page.offset())
            {
                ++index;
                continue;
            }
            if (index >= page.offset() + page.page_size)
                break;
            ++index;

            std::string status = "available";
            if (bonus.order_id > 0)
                status = "used";
            else if (bonus.use_end_date > 0 && now > bonus.use_end_date)
                status = "expired";
            else if (bonus.use_start_date > 0 && now < bonus.use_start_date)
                status = "not_started";

            wfrest::Json::Object item;
            item.push_back("bonus_id", bonus.bonus_id);
            item.push_back("bonus_sn", bonus.bonus_sn);
            item.push_back("money", bonus.money);
            item.push_back("currency", "CNY");
            item.push_back("status", status);
            item.push_back("use_start_date", shared::isoUtc(bonus.use_start_date));
            item.push_back("use_end_date", shared::isoUtc(bonus.use_end_date));
            if (status == "used")
                item.push_back("order_id", bonus.order_id);
            items.push_back(item);
        }

        api::send(req, resp, ApiResponse::ok(api::listBody(page, total, items)));
    });

    // POST /api/v1/me/email-verifications — queue a verification email
    sv.POST("/api/v1/me/email-verifications",
            [users, tokens](const wfrest::HttpReq *req, wfrest::HttpResp *resp)
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

        // plaintext token goes only to the outbox payload, never to the response
        std::string token = shared::randomTokenHex(32);
        int64_t expires_at = shared::nowUnix() + 24 * 3600;

        tokens->replaceEmailVerification(*user_id, shared::sha256Hex(token), expires_at);
        tokens->queueEmail(*user_id, user->email, "verify_email",
                           "{\"token\":\"" + token + "\"}");

        wfrest::Json::Object out;
        out.push_back("status", "queued");
        api::send(req, resp, ApiResponse::accepted(out));
    });

    // POST /api/v1/me/account/requests — deposit / withdrawal request
    sv.POST("/api/v1/me/account/requests",
            [users, account](const wfrest::HttpReq *req, wfrest::HttpResp *resp)
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

        std::string kind;
        if (!api::readStr(body, "kind", kind) ||
            (kind != "deposit" && kind != "withdrawal"))
        {
            wfrest::Json::Object details;
            details.push_back("field", "kind");
            api::send(req, resp,
                      ApiError::validationError("kind must be deposit or withdrawal", details));
            return;
        }

        std::string amount_text;
        int64_t amount_cents = 0;
        if (!api::readStr(body, "amount", amount_text) ||
            !shared::Money::parse(amount_text, amount_cents) || amount_cents <= 0)
        {
            wfrest::Json::Object details;
            details.push_back("field", "amount");
            api::send(req, resp,
                      ApiError::validationError("amount must be a positive decimal string",
                                                details));
            return;
        }

        std::string note;
        if (body.has("note") && !api::readStr(body, "note", note))
        {
            wfrest::Json::Object details;
            details.push_back("field", "note");
            api::send(req, resp, ApiError::validationError("note must be a string", details));
            return;
        }
        if (note.size() > 255)
        {
            wfrest::Json::Object details;
            details.push_back("field", "note");
            api::send(req, resp, ApiError::validationError("note too long", details));
            return;
        }

        int64_t payment_id = 0;
        if (kind == "deposit")
        {
            if (!api::readInt(body, "payment_id", payment_id) || payment_id <= 0)
            {
                wfrest::Json::Object details;
                details.push_back("field", "payment_id");
                api::send(req, resp,
                          ApiError::validationError("payment_id is required for deposit",
                                                    details));
                return;
            }
            if (!account->paymentEnabled(payment_id))
            {
                wfrest::Json::Object details;
                details.push_back("field", "payment_id");
                api::send(req, resp,
                          ApiError::validationError("payment method is not enabled", details));
                return;
            }
        }

        int64_t request_id = 0;
        std::string status;
        if (kind == "deposit")
        {
            request_id = account->createDepositRequest(*user_id, amount_cents, payment_id, note);
            status = "pending_payment";
        }
        else
        {
            if (!account->createWithdrawalRequest(*user_id, amount_cents, note, request_id))
            {
                api::send(req, resp,
                          ApiError::outOfStock("insufficient available balance"));
                return;
            }
            status = "pending_review";
        }

        wfrest::Json::Object out;
        out.push_back("id", request_id);
        out.push_back("kind", kind);
        out.push_back("amount", shared::Money::format(amount_cents));
        out.push_back("currency", "CNY");
        out.push_back("status", status);
        out.push_back("payment_id", payment_id);
        out.push_back("note", note);
        out.push_back("created_at", shared::isoUtc(shared::nowUnix()));
        api::send(req, resp, ApiResponse::created(out));
    });

    // GET /api/v1/me/account/requests — deposit/withdrawal requests + balances
    sv.GET("/api/v1/me/account/requests",
           [users, account](const wfrest::HttpReq *req, wfrest::HttpResp *resp)
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

        domain::AccountRequestPage result =
            account->listRequests(*user_id, page.offset(), page.page_size);

        wfrest::Json::Array items;
        for (const domain::AccountRequest &request : result.items)
        {
            wfrest::Json::Object item;
            item.push_back("id", request.id);
            item.push_back("kind", request.kind);
            item.push_back("amount", request.amount);
            item.push_back("currency", "CNY");
            item.push_back("status", request.status);
            item.push_back("payment_id", request.payment_id);
            item.push_back("note", request.note);
            item.push_back("created_at",
                           shared::isoUtc(std::strtoll(request.created_at.c_str(), nullptr, 10)));
            if (!request.paid_at.empty())
                item.push_back("paid_at",
                               shared::isoUtc(std::strtoll(request.paid_at.c_str(), nullptr, 10)));
            items.push_back(item);
        }

        wfrest::Json out = api::listBody(page, result.total, items);
        out.push_back("available_balance", result.balance.available);
        out.push_back("frozen_balance", result.balance.frozen);
        api::send(req, resp, ApiResponse::ok(out));
    });

    // DELETE /api/v1/me/account/requests/{id} — cancel an unprocessed request
    sv.DELETE("/api/v1/me/account/requests/{id}",
              [users, account](const wfrest::HttpReq *req, wfrest::HttpResp *resp)
    {
        api::ApiResponse err;
        std::optional<int64_t> user_id = api::authenticate(req, users, err);
        if (!user_id)
        {
            api::send(req, resp, err);
            return;
        }

        int64_t request_id = 0;
        if (!api::parsePathId(req, "id", request_id))
        {
            wfrest::Json::Object details;
            details.push_back("field", "id");
            api::send(req, resp, ApiError::validationError("id must be a positive integer", details));
            return;
        }

        if (!account->cancelRequest(*user_id, request_id))
        {
            api::send(req, resp, ApiError::notFound("request not found or not cancellable"));
            return;
        }

        api::send(req, resp, ApiResponse::noContent());
    });
}

} // namespace ecshop::http
