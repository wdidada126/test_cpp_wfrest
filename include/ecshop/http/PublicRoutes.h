#pragma once

#include "ecshop/infrastructure/db/Db.h"

#include "wfrest/HttpServer.h"

#include <memory>

namespace ecshop::http {

// Document-type endpoints: /goods-widget.js, /sitemap.xml, /feed.xml,
// /cycle-image.xml.
void registerPublicRoutes(wfrest::HttpServer &sv, std::shared_ptr<infra::Db> db);

} // namespace ecshop::http
