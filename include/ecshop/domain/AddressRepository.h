#pragma once

#include "ecshop/domain/Address.h"

#include <memory>
#include <optional>
#include <vector>

namespace ecshop::domain {

class AddressRepository
{
public:
    virtual ~AddressRepository() = default;

    virtual std::vector<Address> listOfUser(int64_t user_id) = 0;

    virtual int64_t create(int64_t user_id, const Address &address) = 0;

    // owner-scoped edit; false when the row belongs to nobody but this user
    virtual bool update(int64_t user_id, int64_t address_id, const Address &address) = 0;

    virtual bool remove(int64_t user_id, int64_t address_id) = 0;
};

} // namespace ecshop::domain
