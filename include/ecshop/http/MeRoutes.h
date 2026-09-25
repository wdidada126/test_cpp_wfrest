#pragma once

#include "ecshop/infrastructure/db/Db.h"

#include "wfrest/HttpServer.h"

#include <memory>

namespace ecshop::http {

// Current-user URLs under /api/v1/me.
void registerMeRoutes(wfrest::HttpServer &sv, std::shared_ptr<infra::Db> db);

} // namespace ecshop::http
