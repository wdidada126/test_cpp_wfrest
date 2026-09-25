#include "ecshop/http/PackageRoutes.h"
#include "ecshop/http/HttpUtil.h"
#include "ecshop/infrastructure/SqlPackageRepository.h"

namespace ecshop::http {

static wfrest::Json packageToJson(const domain::Package &package)
{
    wfrest::Json::Object obj;
    obj.push_back("act_id", package.act_id);
    obj.push_back("name", package.name);
    obj.push_back("description", package.description);
    obj.push_back("goods_number", package.goods_number);
    obj.push_back("package_price", package.package_price);
    obj.push_back("subtotal", package.subtotal);
    obj.push_back("saving", package.saving);
    obj.push_back("currency", "CNY");
    obj.push_back("start_time", package.start_time);
    obj.push_back("end_time", package.end_time);

    wfrest::Json::Array items;
    for (const domain::PackageItem &item : package.items)
    {
        wfrest::Json::Object it;
        it.push_back("goods_id", item.goods_id);
        it.push_back("name", item.name);
        it.push_back("price", item.price);
        it.push_back("quantity", item.quantity);
        items.push_back(it);
    }
    obj.push_back("items", items);
    return obj;
}

void registerPackageRoutes(wfrest::HttpServer &sv, std::shared_ptr<infra::Db> db)
{
    auto packages = std::make_shared<infra::SqlPackageRepository>(db);

    // GET /api/v1/packages — active gift packages
    sv.GET("/api/v1/packages", [packages](const wfrest::HttpReq *req, wfrest::HttpResp *resp)
    {
        api::PageQuery page;
        api::ApiResponse err;
        if (!api::parsePage(req, page, err))
        {
            api::send(req, resp, err);
            return;
        }

        // take the requested page slice from a bounded active set
        std::vector<domain::Package> all = packages->listActive(100);

        wfrest::Json::Array items;
        int64_t index = 0;
        for (const domain::Package &package : all)
        {
            if (index < page.offset())
            {
                ++index;
                continue;
            }
            if (index >= page.offset() + page.page_size)
                break;
            ++index;
            items.push_back(packageToJson(package));
        }

        api::send(req, resp,
                  ApiResponse::ok(api::listBody(page, static_cast<int64_t>(all.size()), items)));
    });
}

} // namespace ecshop::http
