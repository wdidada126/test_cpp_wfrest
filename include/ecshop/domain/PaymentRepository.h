#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace ecshop::domain {

enum class PaymentCallbackStatus
{
    Completed,   // order moved to paid (first submission)
    Replayed,    // same provider + trade_no + amount, already recorded
    Conflict,    // same trade_no used for another order/amount
    OrderNotFound,
    AmountMismatch,
    InvalidState, // order not pending_payment
};

class PaymentRepository
{
public:
    virtual ~PaymentRepository() = default;

    // verifies nothing; the HTTP layer checked signature and body first.
    // Realizes the payment: pay_log row + pending_payment -> paid in one
    // transaction.
    virtual PaymentCallbackStatus applyCallback(const std::string &provider,
                                                const std::string &provider_trade_no,
                                                const std::string &order_sn,
                                                int64_t amount_cents,
                                                const std::string &raw_payload) = 0;
};

} // namespace ecshop::domain
