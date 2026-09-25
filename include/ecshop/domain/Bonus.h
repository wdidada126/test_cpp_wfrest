#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace ecshop::domain {

struct BonusRecord
{
    int64_t bonus_id = 0;
    int64_t bonus_type_id = 0;
    std::string bonus_sn;
    std::string money; // decimal string, from bonus_type.type_money
    int64_t use_start_date = 0;
    int64_t use_end_date = 0;
    int64_t user_id = 0;
    int64_t used_time = 0;
    int64_t order_id = 0;
};

} // namespace ecshop::domain
