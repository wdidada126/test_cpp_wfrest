#include "ecshop/http/HttpUtil.h"

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <random>
#include <sstream>

namespace ecshop::api {

std::string requestId(const wfrest::HttpReq *req)
{
    const std::string &provided = req->header("X-Request-Id");
    if (!provided.empty())
        return provided;

    static std::atomic<uint64_t> counter{0};
    static const uint64_t salt = [] {
        std::random_device rd;
        return (static_cast<uint64_t>(rd()) << 32) ^ std::random_device{}();
    }();

    uint64_t n = counter.fetch_add(1) + 1;
    char buf[40];
    std::snprintf(buf, sizeof(buf), "%llx-%llx",
                  static_cast<unsigned long long>(salt),
                  static_cast<unsigned long long>(n));
    return std::string(buf);
}

void send(const wfrest::HttpReq *req, wfrest::HttpResp *resp, const ApiResponse &res)
{
    resp->set_status(res.status);

    if (!res.has_body && !res.is_error)
        return;

    if (res.is_error)
    {
        wfrest::Json::Object err;
        err.push_back("code", res.code);
        err.push_back("message", res.message);
        err.push_back("request_id", requestId(req));
        err.push_back("details", res.details);
        resp->Json(err);
        return;
    }

    resp->Json(res.body);
}

bool parsePathId(const wfrest::HttpReq *req, const std::string &name, int64_t &out)
{
    const std::string &raw = req->param(name);
    if (raw.empty())
        return false;

    char *end = nullptr;
    long long v = std::strtoll(raw.c_str(), &end, 10);
    if (!end || *end != '\0' || v <= 0)
        return false;

    out = v;
    return true;
}

bool parsePage(const wfrest::HttpReq *req, PageQuery &out, ApiResponse &err)
{
    out.page = 1;
    out.page_size = 20;

    auto readPositive = [&](const char *key, int64_t &target, int64_t max) -> bool {
        const std::string &raw = req->query(key);
        if (raw.empty())
            return true;

        char *end = nullptr;
        long long v = std::strtoll(raw.c_str(), &end, 10);
        if (!end || *end != '\0' || v < 1 || v > max)
        {
            wfrest::Json::Object details;
            details.push_back("field", std::string(key));
            err = ApiError::validationError(std::string(key) + " out of range", details);
            return false;
        }
        target = v;
        return true;
    };

    if (!readPositive("page", out.page, 1000000))
        return false;
    if (!readPositive("page_size", out.page_size, 100))
        return false;

    return true;
}

wfrest::Json listBody(const PageQuery &page, int64_t total, const wfrest::Json &items)
{
    wfrest::Json::Object body;
    body.push_back("page", page.page);
    body.push_back("page_size", page.page_size);
    body.push_back("total", total);
    body.push_back("items", items);
    return body;
}

bool parseJsonBody(const wfrest::HttpReq *req, wfrest::Json &out, ApiResponse &err)
{
    if (req->content_type() != wfrest::APPLICATION_JSON)
    {
        err = ApiError::validationError("Content-Type must be application/json");
        return false;
    }

    wfrest::Json &body = req->json();
    if (!body.is_valid() || !body.is_object())
    {
        err = ApiError::validationError("request body must be a JSON object");
        return false;
    }

    out = body;
    return true;
}

bool readInt(const wfrest::Json &body, const std::string &key, int64_t &out)
{
    if (!body.has(key))
        return false;
    wfrest::Json v = body[key];
    if (!v.is_number())
        return false;
    out = static_cast<int64_t>(v.get<double>());
    return true;
}

bool readStr(const wfrest::Json &body, const std::string &key, std::string &out)
{
    if (!body.has(key))
        return false;
    wfrest::Json v = body[key];
    if (v.type() != JSON_VALUE_STRING)
        return false;
    out = v.get<std::string>();
    return true;
}

bool readBool(const wfrest::Json &body, const std::string &key, bool &out)
{
    if (!body.has(key))
        return false;
    wfrest::Json v = body[key];
    if (!v.is_boolean())
        return false;
    out = v.get<bool>();
    return true;
}

} // namespace ecshop::api
