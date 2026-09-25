#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace ecshop::domain {

// Catalog entities. Pure data: no JSON, no HTTP, no SQL types.

struct GoodsImage
{
    int64_t img_id = 0;
    std::string url;
    std::string thumb_url;
    std::string original_url;
    std::string description;
};

// 规格 (goods_attr)
struct GoodsSpec
{
    int64_t goods_attr_id = 0;
    int64_t attr_id = 0;
    std::string value;
    std::string price; // decimal string
};

// SKU (products)
struct GoodsProduct
{
    int64_t product_id = 0;
    std::string product_sn;
    int64_t stock = 0;
    std::vector<int64_t> attribute_ids;
};

struct Goods
{
    int64_t goods_id = 0;
    int64_t cat_id = 0;
    int64_t brand_id = 0;
    std::string goods_sn;
    std::string name;
    std::string brief;
    std::string description;
    std::string price;        // shop_price, decimal string
    std::string market_price; // decimal string
    int64_t stock_available = 0;
    bool is_on_sale = false;
    std::string category_name;
    std::string brand_name;
    std::vector<GoodsImage> images;
    std::vector<GoodsSpec> specs;
    std::vector<GoodsProduct> products;
};

// Row shape shared by all goods listings (search, category goods, home, ...)
struct GoodsSummary
{
    int64_t goods_id = 0;
    std::string goods_sn;
    std::string name;
    std::string brief;
    std::string price;
    std::string market_price;
};

// GET /api/v1/quotation rows: goods + SKU quote line
struct QuotationRow
{
    int64_t goods_id = 0;
    std::string goods_sn;
    int64_t product_id = 0;
    std::string product_sn;
    std::string category;
    std::string price; // base shop price, decimal string
    int64_t stock = 0;
    std::vector<int64_t> attribute_ids;
};

struct QuotationPage
{
    int64_t total = 0;
    std::vector<QuotationRow> items;
};

// GET /api/v1/goods/{id}/gallery payload
struct GoodsGallery
{
    int64_t goods_id = 0;
    std::string name;
    std::vector<GoodsImage> images;
};

// Paged listing result (docs/03_api_contract.md list envelope)
struct GoodsPage
{
    int64_t total = 0;
    std::vector<GoodsSummary> items;
};

struct CategorySummary
{
    int64_t cat_id = 0;
    int64_t parent_id = 0;
    std::string name;
    int64_t goods_count = 0;
};

struct BrandSummary
{
    int64_t brand_id = 0;
    std::string name;
    std::string logo;
    std::string site_url;
    int64_t goods_count = 0;
};

struct BrandPage
{
    int64_t total = 0;
    std::vector<BrandSummary> items;
};

// GET /api/v1/goods filters. 0 means "no filter".
struct GoodsFilter
{
    std::string q;
    int64_t category_id = 0;
    int64_t brand_id = 0;
    std::string sort; // price_asc|price_desc|newest|sales, empty = goods_id
};

} // namespace ecshop::domain
