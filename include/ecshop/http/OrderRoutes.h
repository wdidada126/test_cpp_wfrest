#pragma once

#include "ecshop/infrastructure/db/Db.h"

#include "wfrest/HttpServer.h"

#include <memory>

namespace ecshop::http {

// Order URLs under /api/v1.
void registerOrderRoutes(wfrest::HttpServer &sv, std::shared_ptr<infra::Db> db);

} // namespace ecshop::http
