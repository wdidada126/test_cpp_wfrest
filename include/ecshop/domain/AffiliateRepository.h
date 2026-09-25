#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace ecshop::domain {

struct AffiliateLevel
{
    int64_t level = 0; // 1..10 hops under the current user
    int64_t users = 0;
};

// merged referral order rows: sub-level orders plus the current user's share
struct AffiliateOrderRow
{
    int64_t order_id = 0;
    std::string order_sn_masked; // same masking rule as the legacy page
    std::string order_amount;    // decimal string
    bool separated = false;
    std::string money; // share from affiliate_log when separated
    int64_t point = 0;
    int64_t separate_type = 0;
    std::string created_at; // unix seconds as text
};

class AffiliateRepository
{
public:
    virtual ~AffiliateRepository() = default;

    virtual std::vector<AffiliateLevel> levelsOf(int64_t user_id) = 0;
    virtual std::vector<AffiliateOrderRow> ordersOf(int64_t user_id, int64_t offset,
                                                    int64_t limit) = 0;
    virtual int64_t ordersCount(int64_t user_id) = 0;
};

} // namespace ecshop::domain
