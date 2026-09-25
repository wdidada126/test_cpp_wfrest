#pragma once

#include "ecshop/domain/Order.h"
#include "ecshop/infrastructure/db/Db.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

// NOTE: this header intentionally lives next to the order domain; the group
// buy rows join user orders with goods_activity.act_type = 1.

namespace ecshop::domain {

struct MyGroupBuy
{
    int64_t act_id = 0;
    std::string act_name;
    int64_t start_time = 0;
    int64_t end_time = 0;
    int64_t order_id = 0;
    std::string order_sn;
    std::string order_amount; // order money snapshot, decimal string
    std::string order_status;
};

class GroupBuyRepository
{
public:
    virtual ~GroupBuyRepository() = default;

    virtual std::vector<MyGroupBuy> listOfUser(int64_t user_id) = 0;
    virtual std::optional<MyGroupBuy> findOfUser(int64_t user_id, int64_t act_id) = 0;
};

} // namespace ecshop::domain
