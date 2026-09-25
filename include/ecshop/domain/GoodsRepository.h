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
};

} // namespace ecshop::domain
