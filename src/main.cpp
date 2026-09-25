#include "wfrest/HttpServer.h"
#include "wfrest/Json.h"
#include <signal.h>
#include <unistd.h>

using namespace wfrest;

static void sig_handler(int signo)
{
    kill(getpid(), SIGTERM);
}

int main(int argc, char **argv)
{
    HttpServer sv;

    // GET /ping -> pong
    sv.GET("/ping", [](const HttpReq *req, HttpResp *resp)
    {
        resp->set_status(HttpStatusOK);
        resp->set_content("pong\n", "text/plain");
    });

    // GET /json -> JSON response
    sv.GET("/json", [](const HttpReq *req, HttpResp *resp)
    {
        Json json;
        json["message"] = "Hello, wfrest!";
        json["status"]  = "ok";
        resp->set_status(HttpStatusOK);
        resp->set_content(json.dump(), "application/json");
    });

    // POST /echo -> echo back POST body
    sv.POST("/echo", [](const HttpReq *req, HttpResp *resp)
    {
        resp->set_status(HttpStatusOK);
        resp->set_content(req->body(), "text/plain");
    });

    uint16_t port = argc > 1 ? static_cast<uint16_t>(atoi(argv[1])) : 8080;

    if (sv.start(port) == 0)
    {
        signal(SIGINT, sig_handler);
        sv.wait();
    }
    else
    {
        fprintf(stderr, "Failed to start server on port %u\n", port);
        return 1;
    }

    return 0;
}
