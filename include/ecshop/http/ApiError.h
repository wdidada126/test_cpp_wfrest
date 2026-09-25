#pragma once

#include "wfrest/Json.h"

#include <string>
#include <string_view>

namespace ecshop::api {

// Response handed to the HTTP adapter. Success bodies are the resource itself
// (docs/03_api_contract.md); errors carry the fixed envelope fields and are
// serialized by send() so request_id can be filled in one place.
struct ApiResponse
{
    int status = 200;
    bool has_body = true;
    bool is_error = false;

    wfrest::Json body; // success resource

    std::string code;    // error: machine readable code
    std::string message; // error: client facing message
    wfrest::Json details; // error: optional context, e.g. {"field":"quantity"}

    static ApiResponse ok(wfrest::Json body);
    static ApiResponse created(wfrest::Json body);
    static ApiResponse noContent();
};

// Central factory for JSON error responses so status codes, machine readable
// codes and client messages stay consistent (docs/10_api_error_factories.md).
class ApiError
{
public:
    // 400
    [[nodiscard]] static ApiResponse validationError(std::string_view message,
                                                     const wfrest::Json &details = wfrest::Json::Object());
    // 401
    [[nodiscard]] static ApiResponse unauthenticated(std::string_view message);
    // 403
    [[nodiscard]] static ApiResponse forbidden(std::string_view message);
    // 404
    [[nodiscard]] static ApiResponse notFound(std::string_view message);
    // 409, `code` drives client branching, `message` is for humans
    [[nodiscard]] static ApiResponse conflict(std::string_view code, std::string_view message);
    // 409 semantic shortcut, delegates to conflict("out_of_stock", ...)
    [[nodiscard]] static ApiResponse outOfStock(std::string_view message);
    // 422
    [[nodiscard]] static ApiResponse invalidState(std::string_view message);
    // 500
    [[nodiscard]] static ApiResponse internalError(std::string_view message);

private:
    static ApiResponse make(int status, std::string_view code, std::string_view message,
                            const wfrest::Json &details);
};

} // namespace ecshop::api
