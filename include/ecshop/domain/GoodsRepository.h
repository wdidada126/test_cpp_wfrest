#pragma once

#include "ecshop/domain/Catalog.h"

#include <memory>
#include <optional>

namespace ecshop::domain {

// Data source for catalog reads. Visibility rule for public reads:
// is_on_sale = 1 AND is_delete = 0.
class GoodsRepository
{
public:
    virtual ~GoodsRepository() = default;

    // nullopt when missing, deleted or off shelf
    virtual std::optional<Goods> findVisibleGoods(int64_t goods_id) = 0;

    // visible category means existing and is_show = 1
    virtual bool categoryVisible(int64_t cat_id) = 0;

    // on-sale, not deleted goods of one category
    virtual GoodsPage listCategoryGoods(int64_t cat_id, int64_t offset, int64_t limit) = 0;

    // on-sale, not deleted goods matching the search filters
    virtual GoodsPage searchGoods(const GoodsFilter &filter, int64_t offset, int64_t limit) = 0;

    // newest on-sale goods for the home page
    virtual std::vector<GoodsSummary> listNewestGoods(int64_t limit) = 0;

    // visible categories with goods_count (on-sale, not deleted, alone-sale)
    virtual std::vector<CategorySummary> listVisibleCategories() = 0;
};

} // namespace ecshop::domain
