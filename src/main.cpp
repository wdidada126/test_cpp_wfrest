#include "ecshop/app/AppConfig.h"
#include "ecshop/http/AddressRoutes.h"
#include "ecshop/http/ArticleRoutes.h"
#include "ecshop/http/AuthRoutes.h"
#include "ecshop/http/BookingRoutes.h"
#include "ecshop/http/CartRoutes.h"
#include "ecshop/http/CatalogRoutes.h"
#include "ecshop/http/FavoriteRoutes.h"
#include "ecshop/http/HealthRoutes.h"
#include "ecshop/http/MeRoutes.h"
#include "ecshop/http/RegionRoutes.h"
#include "ecshop/http/TagRoutes.h"
#include "ecshop/infrastructure/db/DbFactory.h"

#include "wfrest/HttpServer.h"
#include "wfrest/Json.h"
#include <signal.h>
#include <unistd.h>

using namespace wfrest;

static HttpServer *g_sv = nullptr;

static void sig_handler(int signo)
{
    if (g_sv)
        g_sv->stop();
}

int main(int argc, char **argv)
{
    ecshop::app::AppConfig cfg = ecshop::app::AppConfig::fromEnv();

    std::shared_ptr<ecshop::infra::Db> db;
    try
    {
        db = ecshop::infra::openDb(cfg);
    }
    catch (const std::exception &e)
    {
        fprintf(stderr, "failed to open database: %s\n", e.what());
        return 1;
    }

    HttpServer sv;

    ecshop::http::registerHealthRoutes(sv, db);
    ecshop::http::registerCatalogRoutes(sv, db);
    ecshop::http::registerArticleRoutes(sv, db);
    ecshop::http::registerAuthRoutes(sv, db);
    ecshop::http::registerMeRoutes(sv, db);
    ecshop::http::registerAddressRoutes(sv, db);
    ecshop::http::registerFavoriteRoutes(sv, db);
    ecshop::http::registerCartRoutes(sv, db);
    ecshop::http::registerTagRoutes(sv, db);
    ecshop::http::registerBookingRoutes(sv, db);
    ecshop::http::registerRegionRoutes(sv, db);

    // GET /ping -> pong
    sv.GET("/ping", [](const HttpReq *req, HttpResp *resp)
    {
        resp->String("pong\n");
    });

    // GET /json -> JSON response
    sv.GET("/json", [](const HttpReq *req, HttpResp *resp)
    {
        Json json;
        json["message"] = "Hello, wfrest!";
        json["status"]  = "ok";
        resp->Json(json);
    });

    // POST /echo -> echo back POST body
    sv.POST("/echo", [](const HttpReq *req, HttpResp *resp)
    {
        resp->String(req->body());
    });

    uint16_t port = argc > 1 ? static_cast<uint16_t>(atoi(argv[1])) : 18080;

    if (sv.start(port) == 0)
    {
        g_sv = &sv;
        signal(SIGINT, sig_handler);
        signal(SIGTERM, sig_handler);
        sv.wait_finish();
    }
    else
    {
        fprintf(stderr, "Failed to start server on port %u\n", port);
        return 1;
    }

    return 0;
}
