#include "ecshop/http/ApiError.h"

namespace ecshop::api {

ApiResponse ApiResponse::ok(wfrest::Json body)
{
    ApiResponse res;
    res.status = 200;
    res.body = std::move(body);
    return res;
}

ApiResponse ApiResponse::created(wfrest::Json body)
{
    ApiResponse res;
    res.status = 201;
    res.body = std::move(body);
    return res;
}

ApiResponse ApiResponse::accepted(wfrest::Json body)
{
    ApiResponse res;
    res.status = 202;
    res.body = std::move(body);
    return res;
}

ApiResponse ApiResponse::noContent()
{
    ApiResponse res;
    res.status = 204;
    res.has_body = false;
    return res;
}

ApiResponse ApiError::make(int status, std::string_view code, std::string_view message,
                           const wfrest::Json &details)
{
    ApiResponse res;
    res.status = status;
    res.is_error = true;
    res.code = std::string(code);
    res.message = std::string(message);
    res.details = details;
    return res;
}

ApiResponse ApiError::validationError(std::string_view message, const wfrest::Json &details)
{
    return make(400, "validation_error", message, details);
}

ApiResponse ApiError::unauthenticated(std::string_view message)
{
    return make(401, "unauthenticated", message, wfrest::Json::Object());
}

ApiResponse ApiError::forbidden(std::string_view message)
{
    return make(403, "forbidden", message, wfrest::Json::Object());
}

ApiResponse ApiError::notFound(std::string_view message)
{
    return make(404, "not_found", message, wfrest::Json::Object());
}

ApiResponse ApiError::conflict(std::string_view code, std::string_view message)
{
    return make(409, code, message, wfrest::Json::Object());
}

ApiResponse ApiError::outOfStock(std::string_view message)
{
    return conflict("out_of_stock", message);
}

ApiResponse ApiError::invalidState(std::string_view message)
{
    return make(422, "invalid_state", message, wfrest::Json::Object());
}

ApiResponse ApiError::internalError(std::string_view message)
{
    return make(500, "internal_error", message, wfrest::Json::Object());
}

} // namespace ecshop::api
