#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace ecshop::domain {

struct Region
{
    int64_t region_id = 0;
    int64_t parent_id = 0;
    std::string name;
    int64_t region_type = 0;
};

struct RegionPage
{
    int64_t total = 0;
    std::vector<Region> items;
};

} // namespace ecshop::domain
