#pragma once

#include "ecshop/domain/Order.h"
#include "ecshop/infrastructure/db/Db.h"

#include <memory>

namespace ecshop::infra {

class SqlOrderRepository : public domain::OrderRepository
{
public:
    explicit SqlOrderRepository(std::shared_ptr<Db> db) : db_(std::move(db)) {}

    domain::PlaceOrderStatus placeOrder(const domain::PlaceOrderCommand &command,
                                        domain::OrderSummary &out) override;

private:
    std::string makeOrderSn();
    std::shared_ptr<Db> db_;
};

} // namespace ecshop::infra
