#pragma once

#include "ecshop/infrastructure/db/Db.h"

#include "wfrest/HttpServer.h"

#include <memory>

namespace ecshop::http {

// Auth/account URLs under /api/v1.
void registerAuthRoutes(wfrest::HttpServer &sv, std::shared_ptr<infra::Db> db);

} // namespace ecshop::http
