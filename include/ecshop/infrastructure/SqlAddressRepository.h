#pragma once

#include "ecshop/domain/AddressRepository.h"
#include "ecshop/infrastructure/db/Db.h"

namespace ecshop::infra {

class SqlAddressRepository : public domain::AddressRepository
{
public:
    explicit SqlAddressRepository(std::shared_ptr<Db> db) : db_(std::move(db)) {}

    std::vector<domain::Address> listOfUser(int64_t user_id) override;
    int64_t create(int64_t user_id, const domain::Address &address) override;
    bool update(int64_t user_id, int64_t address_id, const domain::Address &address) override;
    bool remove(int64_t user_id, int64_t address_id) override;

private:
    std::shared_ptr<Db> db_;
};

} // namespace ecshop::infra
