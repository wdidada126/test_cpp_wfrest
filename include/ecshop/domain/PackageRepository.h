#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace ecshop::domain {

struct PackageItem
{
    int64_t goods_id = 0;
    std::string name;
    std::string price; // current shop price, decimal string
    int64_t quantity = 0;
};

struct Package
{
    int64_t act_id = 0;
    std::string name;
    std::string description;
    int64_t goods_number = 0; // per package, sum of item quantities
    std::string package_price; // decimal string from ext_info JSON
    std::string subtotal;      // sum of current prices
    std::string saving;        // max(0, subtotal - package_price)
    std::string start_time;    // ISO
    std::string end_time;      // ISO
    std::vector<PackageItem> items;
};

class PackageRepository
{
public:
    virtual ~PackageRepository() = default;

    virtual std::vector<Package> listActive(int64_t limit) = 0;
};

} // namespace ecshop::domain
