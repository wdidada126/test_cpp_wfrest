#pragma once

#include "ecshop/domain/PromotionRepository.h"
#include "ecshop/infrastructure/db/Db.h"

#include <memory>

namespace ecshop::infra {

class SqlPromotionRepository : public domain::PromotionRepository
{
public:
    explicit SqlPromotionRepository(std::shared_ptr<Db> db) : db_(std::move(db)) {}

    domain::PromotionPage listActive(int64_t act_type, int64_t offset, int64_t limit) override;
    std::optional<domain::Promotion> findActive(int64_t act_id) override;
    std::vector<domain::Favourable> listFavourableActive(int64_t offset, int64_t limit) override;

private:
    std::shared_ptr<Db> db_;
};

} // namespace ecshop::infra
