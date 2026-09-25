#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace ecshop::domain {

struct ShippingOption
{
    int64_t shipping_id = 0;
    std::string name;
    std::string fee; // decimal string
};

struct PaymentOption
{
    int64_t payment_id = 0;
    std::string name;
    std::string fee; // decimal string
};

class CheckoutRepository
{
public:
    virtual ~CheckoutRepository() = default;

    virtual std::vector<ShippingOption> listShipping() = 0;
    virtual std::vector<PaymentOption> listPayment() = 0;

    // nullopt when missing or disabled
    virtual std::optional<std::string> shippingFee(int64_t shipping_id) = 0;
    virtual std::optional<std::string> paymentFee(int64_t payment_id) = 0;

    virtual bool addressOwned(int64_t user_id, int64_t address_id) = 0;
};

} // namespace ecshop::domain
