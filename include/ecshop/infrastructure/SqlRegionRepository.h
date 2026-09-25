#pragma once

#include "ecshop/domain/RegionRepository.h"
#include "ecshop/infrastructure/db/Db.h"

namespace ecshop::infra {

class SqlRegionRepository : public domain::RegionRepository
{
public:
    explicit SqlRegionRepository(std::shared_ptr<Db> db) : db_(std::move(db)) {}

    domain::RegionPage listRegions(int64_t parent_id, int64_t region_type,
                                   int64_t offset, int64_t limit) override;
    std::optional<domain::Region> find(int64_t region_id) override;

private:
    std::shared_ptr<Db> db_;
};

} // namespace ecshop::infra
