#pragma once

#include <vector>

#include "ecshop/domain/AffiliateRepository.h"
#include "ecshop/infrastructure/db/Db.h"

#include <memory>

namespace ecshop::infra {

class SqlAffiliateRepository : public domain::AffiliateRepository
{
public:
    explicit SqlAffiliateRepository(std::shared_ptr<Db> db) : db_(std::move(db)) {}

    std::vector<domain::AffiliateLevel> levelsOf(int64_t user_id) override;
    std::vector<domain::AffiliateOrderRow> ordersOf(int64_t user_id, int64_t offset,
                                                    int64_t limit) override;
    int64_t ordersCount(int64_t user_id) override;

private:
    std::shared_ptr<Db> db_;
};

} // namespace ecshop::infra
