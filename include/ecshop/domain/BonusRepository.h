#pragma once

#include "ecshop/domain/Bonus.h"

#include <memory>
#include <optional>
#include <vector>

namespace ecshop::domain {

class BonusRepository
{
public:
    virtual ~BonusRepository() = default;

    virtual std::optional<BonusRecord> findBySn(const std::string &bonus_sn) = 0;

    // atomically claim an unclaimed bonus; false when already claimed
    virtual bool claim(int64_t bonus_id, int64_t user_id) = 0;

    virtual std::vector<BonusRecord> listOfUser(int64_t user_id) = 0;
};

} // namespace ecshop::domain
