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

} // namespace ecshop::domain
