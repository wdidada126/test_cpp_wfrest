#pragma once

#include <vector>

#include "ecshop/domain/PackageRepository.h"
#include "ecshop/infrastructure/db/Db.h"

#include <memory>

namespace ecshop::infra {

class SqlPackageRepository : public domain::PackageRepository
{
public:
    explicit SqlPackageRepository(std::shared_ptr<Db> db) : db_(std::move(db)) {}

    std::vector<domain::Package> listActive(int64_t limit) override;

private:
    std::shared_ptr<Db> db_;
};

} // namespace ecshop::infra
