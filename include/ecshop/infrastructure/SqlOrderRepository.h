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
    domain::OrderPage listOfUser(int64_t user_id, int64_t offset, int64_t limit) override;
    std::optional<domain::OrderDetail> findOfUser(int64_t user_id, int64_t order_id) override;
    domain::OrderCancelStatus cancelOfUser(int64_t user_id, int64_t order_id,
                                           domain::OrderSummary &out) override;
    domain::OrderCancelStatus receivedOfUser(int64_t user_id, int64_t order_id,
                                             domain::OrderSummary &out) override;

private:
    std::string makeOrderSn();
    std::shared_ptr<Db> db_;
};

} // namespace ecshop::infra
