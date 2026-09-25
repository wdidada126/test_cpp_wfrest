#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "ecshop/domain/BonusRepository.h"
#include "ecshop/infrastructure/db/Db.h"

namespace ecshop::infra {

class SqlBonusRepository : public domain::BonusRepository
{
public:
    explicit SqlBonusRepository(std::shared_ptr<Db> db) : db_(std::move(db)) {}

    std::optional<domain::BonusRecord> findBySn(const std::string &bonus_sn) override;
    bool claim(int64_t bonus_id, int64_t user_id) override;
    std::vector<domain::BonusRecord> listOfUser(int64_t user_id) override;

private:
    std::shared_ptr<Db> db_;
};

} // namespace ecshop::infra
