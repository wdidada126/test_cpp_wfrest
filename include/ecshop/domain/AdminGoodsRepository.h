#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace ecshop::domain {

// admin goods row: all visibility states (filter flags included)
struct AdminGoodsRow
{
    int64_t goods_id = 0;
    std::string goods_sn;
    std::string name;
    std::string brief;
    std::string price;
    std::string market_price;
    int64_t stock = 0;
    int64_t cat_id = 0;
    int64_t brand_id = 0;
    bool is_on_sale = false;
    bool is_delete = false;
    bool is_best = false;
    bool is_new = false;
    bool is_hot = false;
    bool is_promote = false;
};

struct AdminGoodsPage
{
    int64_t total = 0;
    std::vector<AdminGoodsRow> items;
};

// PATCH payload: only present fields are applied
struct AdminGoodsPatch
{
    std::optional<std::string> name;
    std::optional<std::string> brief;
    std::optional<std::string> description;
    std::optional<std::string> price;
    std::optional<std::string> market_price;
    std::optional<int64_t> stock;
    std::optional<int64_t> cat_id;
    std::optional<int64_t> brand_id;
    std::optional<bool> is_on_sale;
    std::optional<bool> is_best;
    std::optional<bool> is_new;
    std::optional<bool> is_hot;
    std::optional<bool> is_promote;
};

class AdminGoodsRepository
{
public:
    virtual ~AdminGoodsRepository() = default;

    virtual AdminGoodsPage list(const std::string &q, int64_t offset, int64_t limit) = 0;
    virtual std::optional<AdminGoodsRow> find(int64_t goods_id) = 0;

    // create with intersection columns; returns the new goods id
    virtual int64_t create(const AdminGoodsRow &goods, const std::string &description) = 0;

    // patch semantics: only set fields change; false when goods is missing
    virtual bool patch(int64_t goods_id, const AdminGoodsPatch &patch) = 0;

    // soft delete (is_delete = 1)
    virtual bool softDelete(int64_t goods_id) = 0;
};

} // namespace ecshop::domain
