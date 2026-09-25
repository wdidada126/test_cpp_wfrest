#pragma once

#include "ecshop/app/AppConfig.h"
#include "ecshop/infrastructure/db/Db.h"

#include <memory>

namespace ecshop::infra {

// Opens the configured backend ("sqlite" or "mysql") and bootstraps the
// SQLite dev schema when the database is empty.
std::shared_ptr<Db> openDb(const app::AppConfig &cfg);

} // namespace ecshop::infra
