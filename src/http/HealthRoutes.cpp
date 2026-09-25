#include "ecshop/http/HealthRoutes.h"
#include "ecshop/http/HttpUtil.h"

namespace ecshop::http {

void registerHealthRoutes(wfrest::HttpServer &sv, std::shared_ptr<infra::Db> db)
{
    sv.GET("/healthz", [db](const wfrest::HttpReq *req, wfrest::HttpResp *resp)
    {
        wfrest::Json::Object body;
        body.push_back("status", "ok");
        body.push_back("db", std::string(db->driverName()));

        bool db_ok = true;
        try
        {
            db->query("SELECT 1", {});
        }
        catch (const std::exception &)
        {
            db_ok = false;
        }
        body.push_back("db_ok", db_ok);

        if (!db_ok)
        {
            api::send(req, resp,
                      api::ApiError::internalError("database is not reachable"));
            return;
        }

        api::send(req, resp, api::ApiResponse::ok(body));
    });
}

} // namespace ecshop::http
