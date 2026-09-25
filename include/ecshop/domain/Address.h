#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace ecshop::domain {

struct Address
{
    int64_t address_id = 0;
    std::string consignee;
    int64_t country_id = 0;
    int64_t province_id = 0;
    int64_t city_id = 0;
    int64_t district_id = 0;
    std::string address;
    std::string zip;
    std::string mobile;
    bool is_default = false;
};

} // namespace ecshop::domain
