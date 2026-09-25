#pragma once

#include "ecshop/infrastructure/db/Db.h"

#include "wfrest/HttpServer.h"

#include <memory>

namespace ecshop::http {

// Package gift sets (act_type = 4) under /api/v1.
void registerPackageRoutes(wfrest::HttpServer &sv, std::shared_ptr<infra::Db> db);

} // namespace ecshop::http
