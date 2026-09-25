#pragma once

#include "ecshop/http/ApiError.h"

#include "wfrest/HttpMsg.h"

#include <cstdint>
#include <string>

namespace ecshop::api {

// X-Request-Id from the client, or a generated one. Always echoed in errors.
std::string requestId(const wfrest::HttpReq *req);

// Serialize an ApiResponse: fills request_id on errors, sets status and body.
void send(const wfrest::HttpReq *req, wfrest::HttpResp *resp, const ApiResponse &res);

// Path/query primitives -----------------------------------------------

// positive int path parameter ("{id}"); false when missing or not a positive int
bool parsePathId(const wfrest::HttpReq *req, const std::string &name, int64_t &out);

struct PageQuery
{
    int64_t page = 1;
    int64_t page_size = 20;
    int64_t offset() const { return (page - 1) * page_size; }
};

// page >= 1, page_size 1..100 (default 20); errors are validation_error
bool parsePage(const wfrest::HttpReq *req, PageQuery &out, ApiResponse &err);

// {"page":..,"page_size":..,"total":..,"items":[...]}
wfrest::Json listBody(const PageQuery &page, int64_t total, const wfrest::Json &items);

// Request body -------------------------------------------------------

// Content-Type must be application/json and the body a JSON object.
bool parseJsonBody(const wfrest::HttpReq *req, wfrest::Json &out, ApiResponse &err);

// Typed field readers with presence/typing checks for parsed bodies.
bool readInt(const wfrest::Json &body, const std::string &key, int64_t &out);
bool readStr(const wfrest::Json &body, const std::string &key, std::string &out);
bool readBool(const wfrest::Json &body, const std::string &key, bool &out);

} // namespace ecshop::api
