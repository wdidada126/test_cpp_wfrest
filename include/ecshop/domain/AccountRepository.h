#pragma once

#include "ecshop/domain/Account.h"

#include <memory>
#include <optional>
#include <string>

namespace ecshop::domain {

class AccountRepository
{
public:
    virtual ~AccountRepository() = default;

    virtual bool paymentEnabled(int64_t payment_id) = 0;

    // creates the balance row on first use
    virtual void ensureBalance(int64_t user_id) = 0;

    // deposit request, status pending_payment; returns the new request id
    virtual int64_t createDepositRequest(int64_t user_id, int64_t amount_cents,
                                         int64_t payment_id, const std::string &note) = 0;

    // withdrawal: atomically moves available -> frozen; false when funds are
    // insufficient. Inserts request + ledger inside the same transaction.
    virtual bool createWithdrawalRequest(int64_t user_id, int64_t amount_cents,
                                         const std::string &note, int64_t &request_id) = 0;

    // current balances; zero amounts when the user has no rows yet
    virtual Balance balanceOf(int64_t user_id) = 0;

    virtual AccountRequestPage listRequests(int64_t user_id, int64_t offset, int64_t limit) = 0;

    // cancel an unprocessed own request; false when missing or not cancellable.
    // A cancelled withdrawal returns the frozen amount to the balance.
    virtual bool cancelRequest(int64_t user_id, int64_t request_id) = 0;

    virtual AccountTransactionPage listTransactions(int64_t user_id, int64_t offset,
                                                     int64_t limit) = 0;
};

} // namespace ecshop::domain
