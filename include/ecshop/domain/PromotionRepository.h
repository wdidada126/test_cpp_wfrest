#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace ecshop::domain {

// goods_activity row (group buy / snatch / auction / exchange / package)
struct Promotion
{
    int64_t act_id = 0;
    std::string name;
    std::string description;
    int64_t act_type = 0;
    int64_t goods_id = 0;
    std::string goods_name;
    int64_t start_time = 0;
    int64_t end_time = 0;
    std::string ext_info; // raw extension content (JSON)
};

struct PromotionPage
{
    int64_t total = 0;
    std::vector<Promotion> items;
};

class PromotionRepository
{
public:
    virtual ~PromotionRepository() = default;

    // act_type 0 = no filter; only rows in the current time window whose goods
    // are still visible
    virtual PromotionPage listActive(int64_t act_type, int64_t offset, int64_t limit) = 0;
    virtual std::optional<Promotion> findActive(int64_t act_id) = 0;
};

} // namespace ecshop::domain
