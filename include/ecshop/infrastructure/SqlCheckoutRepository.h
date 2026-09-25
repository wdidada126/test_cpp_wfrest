#pragma once

#include <optional>
#include <string>
#include <vector>

#include "ecshop/domain/Checkout.h"
#include "ecshop/infrastructure/db/Db.h"

#include <memory>

namespace ecshop::infra {

class SqlCheckoutRepository : public domain::CheckoutRepository
{
public:
    explicit SqlCheckoutRepository(std::shared_ptr<Db> db) : db_(std::move(db)) {}

    std::vector<domain::ShippingOption> listShipping() override;
    std::vector<domain::PaymentOption> listPayment() override;
    std::optional<std::string> shippingFee(int64_t shipping_id) override;
    std::optional<std::string> paymentFee(int64_t payment_id) override;
    bool addressOwned(int64_t user_id, int64_t address_id) override;

private:
    std::shared_ptr<Db> db_;
};

} // namespace ecshop::infra
