#pragma once

#include "ecshop/domain/Order.h"
#include "ecshop/infrastructure/db/Db.h"

#include <memory>

#include <memory>

namespace ecshop::infra {

class SqlAdminOrderRepository
{
public:
    explicit SqlAdminOrderRepository(std::shared_ptr<Db> db) : db_(std::move(db)) {}

    // all orders regardless of user
    domain::OrderPage list(int64_t offset, int64_t limit);

    // nullopt when missing
    std::optional<domain::OrderDetail> find(int64_t order_id);

    // pending_payment/paid -> shipped with audit; conditional on status
    domain::OrderPatchStatus ship(int64_t order_id);

private:
    std::shared_ptr<Db> db_;
};

} // namespace ecshop::infra
