#pragma once

#include <string>

namespace ecshop::app {

// Immutable configuration resolved once at startup. Business code must not
// read environment variables directly.
struct AppConfig
{
    // ECSHOP_DB_DRIVER: "sqlite" (default) or "mysql"
    std::string db_driver = "sqlite";

    // ECSHOP_SQLITE_PATH: default "data/ecshop.sqlite3"
    std::string sqlite_path = "data/ecshop.sqlite3";

    // mysql connection, ECSHOP_MYSQL_HOST/PORT/USER/PASSWORD/DATABASE
    std::string mysql_host = "127.0.0.1";
    int mysql_port = 3306;
    std::string mysql_user = "ecshop";
    std::string mysql_password;
    std::string mysql_database = "ecshop_cpp";

    // ECSHOP_SITE_BASE_URL: absolute URL root for sitemap/feed links
    std::string site_base_url = "http://localhost:18080";

    static AppConfig fromEnv();
};

} // namespace ecshop::app
