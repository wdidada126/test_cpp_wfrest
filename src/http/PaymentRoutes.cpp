#include "ecshop/http/PaymentRoutes.h"
#include "ecshop/http/HttpUtil.h"
#include "ecshop/infrastructure/SqlPaymentRepository.h"
#include "ecshop/shared/Password.h"

#include <cstdlib>
#include <optional>
#include <string>

namespace ecshop::http {

using api::ApiError;
using api::ApiResponse;

void registerPaymentRoutes(wfrest::HttpServer &sv, std::shared_ptr<infra::Db> db)
{
    auto payments = std::make_shared<infra::SqlPaymentRepository>(db);

    // POST /api/v1/payments/{provider}/callback
    sv.POST("/api/v1/payments/{provider}/callback",
            [payments](const wfrest::HttpReq *req, wfrest::HttpResp *resp)
    {
        // secrets must come from the environment; dev fallback keeps curl tests
        // working but production overrides it
        const char *secret_env = std::getenv("ECSHOP_PAYMENT_CALLBACK_HMAC_SECRET");
        std::string secret = secret_env ? secret_env : "dev-callback-secret";

        const std::string &provider = req->param("provider");
        if (provider.empty() || provider.size() > 60)
        {
            wfrest::Json::Object details;
            details.push_back("field", "provider");
            api::send(req, resp,
                      ApiError::validationError("provider must be 1-60 bytes", details));
            return;
        }

        const std::string &signature = req->header("X-Payment-Signature");
        std::string expected = shared::hmacSha256Hex(secret, req->body());
        if (signature.empty() || signature != expected)
        {
            api::send(req, resp,
                      ApiError::forbidden("payment signature verification failed"));
            return;
        }

        wfrest::Json body;
        api::ApiResponse err;
        if (!api::parseJsonBody(req, body, err))
        {
            api::send(req, resp, err);
            return;
        }

        std::string provider_trade_no, order_sn, amount_text;
        if (!api::readStr(body, "provider_trade_no", provider_trade_no) ||
            provider_trade_no.empty() || provider_trade_no.size() > 120)
        {
            wfrest::Json::Object details;
            details.push_back("field", "provider_trade_no");
            api::send(req, resp,
                      ApiError::validationError("provider_trade_no is required", details));
            return;
        }
        if (!api::readStr(body, "order_sn", order_sn) || order_sn.empty() || order_sn.size() > 40)
        {
            wfrest::Json::Object details;
            details.push_back("field", "order_sn");
            api::send(req, resp, ApiError::validationError("order_sn is required", details));
            return;
        }
        if (!api::readStr(body, "amount", amount_text))
        {
            wfrest::Json::Object details;
            details.push_back("field", "amount");
            api::send(req, resp,
                      ApiError::validationError("amount must be a decimal string", details));
            return;
        }

        int64_t amount_cents = 0;
        if (!shared::Money::parse(amount_text, amount_cents) || amount_cents < 0)
        {
            wfrest::Json::Object details;
            details.push_back("field", "amount");
            api::send(req, resp,
                      ApiError::validationError("amount must be a decimal string", details));
            return;
        }

        domain::PaymentCallbackStatus status = payments->applyCallback(
            provider, provider_trade_no, order_sn, amount_cents, req->body());

        switch (status)
        {
        case domain::PaymentCallbackStatus::Completed:
        case domain::PaymentCallbackStatus::Replayed:
        {
            // both return 200: replays must not surface errors to the gateway
            wfrest::Json::Object out;
            out.push_back("order_sn", order_sn);
            out.push_back("status", "paid");
            api::send(req, resp, ApiResponse::ok(out));
            return;
        }
        case domain::PaymentCallbackStatus::Conflict:
            api::send(req, resp,
                      ApiError::conflict("trade_no_conflict",
                                         "provider trade number used for another payment"));
            return;
        case domain::PaymentCallbackStatus::OrderNotFound:
            api::send(req, resp, ApiError::notFound("order not found"));
            return;
        case domain::PaymentCallbackStatus::AmountMismatch:
            api::send(req, resp,
                      ApiError::conflict("amount_mismatch", "payment amount mismatch"));
            return;
        case domain::PaymentCallbackStatus::InvalidState:
        default:
            api::send(req, resp, ApiError::invalidState("order is not in pending_payment"));
            return;
        }
    });
}

} // namespace ecshop::http
