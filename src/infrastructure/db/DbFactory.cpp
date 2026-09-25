#include "ecshop/infrastructure/db/DbFactory.h"
#include "ecshop/infrastructure/db/MysqlDb.h"
#include "ecshop/infrastructure/db/SqliteDb.h"

namespace ecshop::infra {

std::shared_ptr<Db> openDb(const app::AppConfig &cfg)
{
    if (cfg.db_driver == "sqlite")
    {
        auto db = std::make_shared<SqliteDb>(cfg.sqlite_path);
        db->ensureSchema();
        return db;
    }

    if (cfg.db_driver == "mysql")
    {
        MysqlDb::Options opts;
        opts.host = cfg.mysql_host;
        opts.port = cfg.mysql_port;
        opts.user = cfg.mysql_user;
        opts.password = cfg.mysql_password;
        opts.database = cfg.mysql_database;
        return std::make_shared<MysqlDb>(opts);
    }

    throw DbError("unknown ECSHOP_DB_DRIVER: " + cfg.db_driver +
                  " (expected \"sqlite\" or \"mysql\")");
}

} // namespace ecshop::infra
