#pragma once

#include "ecshop/domain/Region.h"

#include <memory>

namespace ecshop::domain {

class RegionRepository
{
public:
    virtual ~RegionRepository() = default;

    // parent_id / region_type: 0 means "no filter"
    virtual RegionPage listRegions(int64_t parent_id, int64_t region_type,
                                   int64_t offset, int64_t limit) = 0;
};

} // namespace ecshop::domain
