#pragma once

#include "ecshop/app/AppConfig.h"
#include "ecshop/infrastructure/db/Db.h"

#include "wfrest/HttpServer.h"

#include <memory>

namespace ecshop::http {

// Admin APIs under /api/v1/admin/**: role based, audited.
void registerAdminRoutes(wfrest::HttpServer &sv, std::shared_ptr<infra::Db> db,
                         const app::AppConfig &cfg);

} // namespace ecshop::http
