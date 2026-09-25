#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace ecshop::domain {

struct ExchangeGoodsRow
{
    int64_t goods_id = 0;
    std::string goods_sn;
    std::string name;
    std::string category;
    std::string price; // decimal string
    int64_t integral = 0;
    bool is_hot = false;
    std::string last_update; // unix seconds as text
};

struct ExchangeGoodsPage
{
    int64_t total = 0;
    std::vector<ExchangeGoodsRow> items;
};

class ExchangeGoodsRepository
{
public:
    virtual ~ExchangeGoodsRepository() = default;

    struct Query
    {
        int64_t category_id = 0; // includes subcategories
        int64_t integral_min = 0;
        int64_t integral_max = 0; // 0 = unbounded
        std::string sort;         // goods_id|exchange_integral|last_update
        std::string order;        // asc|desc (default asc)
    };

    virtual ExchangeGoodsPage list(const Query &query, int64_t offset, int64_t limit) = 0;
    virtual std::optional<ExchangeGoodsRow> findEnabled(int64_t goods_id) = 0;
};

} // namespace ecshop::domain
