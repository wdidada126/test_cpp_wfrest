#include "ecshop/http/PublicRoutes.h"
#include "ecshop/http/HttpUtil.h"
#include "ecshop/infrastructure/SqlArticleRepository.h"
#include "ecshop/infrastructure/SqlCatalogRepository.h"

#include <algorithm>
#include <cstdlib>
#include <optional>
#include <string>
#include <vector>

namespace ecshop::http {

using api::ApiError;
using api::ApiResponse;

static std::string jsEscape(const std::string &text)
{
    std::string out;
    out.reserve(text.size());
    for (char c : text)
    {
        if (c == '"' || c == '\\')
        {
            out.push_back('\\');
            out.push_back(c);
        }
        else if (c == '\n')
        {
            out += "\\n";
        }
        else if (c == '\r')
        {
            out += "\\r";
        }
        else
        {
            out.push_back(c);
        }
    }
    return out;
}

static const std::vector<std::string> kIntroTypes = {
    "is_best", "is_new", "is_hot", "is_promote", "is_random"};

static std::string xmlEscape(const std::string &text)
{
    std::string out;
    out.reserve(text.size());
    for (char c : text)
    {
        switch (c)
        {
        case '&':
            out += "&amp;";
            break;
        case '<':
            out += "&lt;";
            break;
        case '>':
            out += "&gt;";
            break;
        case '"':
            out += "&quot;";
            break;
        case '\'':
            out += "&apos;";
            break;
        default:
            out.push_back(c);
        }
    }
    return out;
}

static void appendUrl(std::string &xml, const std::string &base, const std::string &path)
{
    xml += "<url><loc>" + xmlEscape(base + path) + "</loc></url>\n";
}

void registerPublicRoutes(wfrest::HttpServer &sv, std::shared_ptr<infra::Db> db,
                          const std::string &site_base_url)
{
    auto catalog = std::make_shared<infra::SqlCatalogRepository>(db);
    auto articles = std::make_shared<infra::SqlArticleRepository>(db);

    // GET /goods-widget.js — public goods widget (docs/03)
    sv.GET("/goods-widget.js",
           [catalog](const wfrest::HttpReq *req, wfrest::HttpResp *resp)
    {
        std::string intro_type = req->query("intro_type");
        if (intro_type.empty())
            intro_type = "is_new";
        if (std::find(kIntroTypes.begin(), kIntroTypes.end(), intro_type) == kIntroTypes.end())
        {
            api::send(req, resp, ApiError::validationError(
                                     "intro_type must be is_best|is_new|is_hot|is_promote|is_random"));
            return;
        }

        int64_t goods_num = 10;
        const std::string &raw_num = req->query("goods_num");
        if (!raw_num.empty())
        {
            char *end = nullptr;
            long long v = std::strtoll(raw_num.c_str(), &end, 10);
            if (!end || *end != '\0' || v < 1 || v > 50)
            {
                api::send(req, resp,
                          ApiError::validationError("goods_num must be between 1 and 50"));
                return;
            }
            goods_num = v;
        }

        domain::GoodsFilter filter;
        for (const std::pair<const char *, int64_t *> param :
             {std::make_pair("cat_id", &filter.category_id),
              std::make_pair("brand_id", &filter.brand_id)})
        {
            const std::string &raw = req->query(param.first);
            if (raw.empty())
                continue;
            char *end = nullptr;
            long long v = std::strtoll(raw.c_str(), &end, 10);
            if (!end || *end != '\0' || v < 0)
            {
                api::send(req, resp,
                          ApiError::validationError(std::string(param.first) + " must be an integer"));
                return;
            }
            *param.second = v;
        }

        // intro_type flags filter through listByIntroType; extra filters apply
        // on top via listPublicGoods only for the flag types
        std::vector<domain::GoodsSummary> goods =
            catalog->listByIntroType(intro_type, goods_num * 2);
        if (filter.category_id > 0 || filter.brand_id > 0)
        {
            std::vector<domain::GoodsSummary> filtered;
            for (const domain::GoodsSummary &item : goods)
            {
                std::optional<domain::Goods> full = catalog->findVisibleGoods(item.goods_id);
                if (!full)
                    continue;
                if (filter.category_id > 0 && full->cat_id != filter.category_id)
                    continue;
                if (filter.brand_id > 0 && full->brand_id != filter.brand_id)
                    continue;
                filtered.push_back(item);
                if (static_cast<int64_t>(filtered.size()) >= goods_num)
                    break;
            }
            goods = std::move(filtered);
        }
        else if (static_cast<int64_t>(goods.size()) > goods_num)
        {
            goods.resize(static_cast<size_t>(goods_num));
        }

        std::string js = "window.cppEcshopGoodsWidget = ";
        js += "{\"intro_type\":\"" + intro_type + "\",\"items\":[";
        bool first = true;
        for (const domain::GoodsSummary &item : goods)
        {
            if (!first)
                js += ",";
            first = false;
            js += "{\"goods_id\":" + std::to_string(item.goods_id) + ",";
            js += "\"name\":\"" + jsEscape(item.name) + "\",";
            js += "\"price\":\"" + item.price + "\"}";
        }
        js += "]};\n";

        resp->add_header("Content-Type", "application/javascript; charset=utf-8");
        resp->String(js);
    });

    // GET /sitemap.xml — home, categories, article categories, goods, articles
    sv.GET("/sitemap.xml",
           [catalog, articles, site_base_url](const wfrest::HttpReq *req, wfrest::HttpResp *resp)
    {
        std::string base = site_base_url;
        while (!base.empty() && base.back() == '/')
            base.pop_back();

        std::string xml = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                          "<urlset xmlns=\"http://www.sitemaps.org/schemas/sitemap/0.9\">\n";
        appendUrl(xml, base, "/");

        for (const domain::CategorySummary &cat : catalog->listVisibleCategories())
            appendUrl(xml, base, "/api/v1/categories/" + std::to_string(cat.cat_id) + "/goods");

        for (const domain::ArticleCategory &cat : articles->listVisibleCategories())
            appendUrl(xml, base,
                      "/api/v1/article-categories/" + std::to_string(cat.cat_id) + "/articles");

        // at most 300 public goods (docs/03)
        for (const domain::GoodsSummary &item : catalog->listPublicGoods({}, 300))
            appendUrl(xml, base, "/api/v1/goods/" + std::to_string(item.goods_id));

        for (const domain::Article &article : articles->listOpenArticles(300))
            appendUrl(xml, base, "/api/v1/articles/" + std::to_string(article.article_id));

        xml += "</urlset>\n";

        resp->add_header("Content-Type", "application/xml; charset=utf-8");
        resp->String(xml);
    });
}

} // namespace ecshop::http
