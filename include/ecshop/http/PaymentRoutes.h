#pragma once

#include "ecshop/infrastructure/db/Db.h"

#include "wfrest/HttpServer.h"

#include <memory>
#include <string>

namespace ecshop::http {

// POST /api/v1/payments/{provider}/callback (docs/03: HMAC-SHA256 signed)
void registerPaymentRoutes(wfrest::HttpServer &sv, std::shared_ptr<infra::Db> db);

} // namespace ecshop::http
