#include "ecshop/app/AppConfig.h"

#include <cstdlib>

namespace ecshop::app {

static std::string envStr(const char *key, const std::string &fallback)
{
    const char *v = std::getenv(key);
    return v ? std::string(v) : fallback;
}

AppConfig AppConfig::fromEnv()
{
    AppConfig cfg;
    cfg.db_driver = envStr("ECSHOP_DB_DRIVER", cfg.db_driver);
    cfg.sqlite_path = envStr("ECSHOP_SQLITE_PATH", cfg.sqlite_path);
    cfg.mysql_host = envStr("ECSHOP_MYSQL_HOST", cfg.mysql_host);
    cfg.mysql_user = envStr("ECSHOP_MYSQL_USER", cfg.mysql_user);
    cfg.mysql_password = envStr("ECSHOP_MYSQL_PASSWORD", cfg.mysql_password);
    cfg.mysql_database = envStr("ECSHOP_MYSQL_DATABASE", cfg.mysql_database);
    cfg.site_base_url = envStr("ECSHOP_SITE_BASE_URL", cfg.site_base_url);

    const char *port = std::getenv("ECSHOP_MYSQL_PORT");
    if (port && *port)
        cfg.mysql_port = std::atoi(port);

    return cfg;
}

} // namespace ecshop::app
