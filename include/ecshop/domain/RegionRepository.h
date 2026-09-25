#pragma once

#include <vector>

#include "ecshop/domain/Region.h"

#include <memory>
#include <optional>

namespace ecshop::domain {

class RegionRepository
{
public:
    virtual ~RegionRepository() = default;

    // parent_id / region_type: 0 means "no filter"
    virtual RegionPage listRegions(int64_t parent_id, int64_t region_type,
                                   int64_t offset, int64_t limit) = 0;

    virtual std::optional<Region> find(int64_t region_id) = 0;

    // direct children ordered by region id (shipping-options cascade)
    virtual std::vector<Region> listChildren(int64_t parent_id) = 0;
};

} // namespace ecshop::domain
