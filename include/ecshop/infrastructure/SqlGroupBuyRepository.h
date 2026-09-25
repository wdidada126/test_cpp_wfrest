#pragma once

#include <optional>
#include <vector>

#include "ecshop/domain/GroupBuyRepository.h"
#include "ecshop/infrastructure/db/Db.h"

#include <memory>

namespace ecshop::infra {

class SqlGroupBuyRepository : public domain::GroupBuyRepository
{
public:
    explicit SqlGroupBuyRepository(std::shared_ptr<Db> db) : db_(std::move(db)) {}

    std::vector<domain::MyGroupBuy> listOfUser(int64_t user_id) override;
    std::optional<domain::MyGroupBuy> findOfUser(int64_t user_id, int64_t act_id) override;

private:
    std::shared_ptr<Db> db_;
};

} // namespace ecshop::infra
