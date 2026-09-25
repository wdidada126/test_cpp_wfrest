#pragma once

#include "ecshop/infrastructure/db/Db.h"

#include "wfrest/HttpServer.h"

#include <memory>
#include <string>

namespace ecshop::http {

// Document-type endpoints: /goods-widget.js, /sitemap.xml, /feed.xml,
// /cycle-image.xml. Absolute URLs come from site.base_url (never Host header).
void registerPublicRoutes(wfrest::HttpServer &sv, std::shared_ptr<infra::Db> db,
                          const std::string &site_base_url);

} // namespace ecshop::http
