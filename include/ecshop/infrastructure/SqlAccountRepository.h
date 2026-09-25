#pragma once

#include "ecshop/domain/AccountRepository.h"
#include "ecshop/infrastructure/db/Db.h"

namespace ecshop::infra {

class SqlAccountRepository : public domain::AccountRepository
{
public:
    explicit SqlAccountRepository(std::shared_ptr<Db> db) : db_(std::move(db)) {}

    bool paymentEnabled(int64_t payment_id) override;
    void ensureBalance(int64_t user_id) override;
    int64_t createDepositRequest(int64_t user_id, int64_t amount_cents,
                                 int64_t payment_id, const std::string &note) override;
    bool createWithdrawalRequest(int64_t user_id, int64_t amount_cents,
                                 const std::string &note, int64_t &request_id) override;
    domain::Balance balanceOf(int64_t user_id) override;
    domain::AccountRequestPage listRequests(int64_t user_id, int64_t offset,
                                            int64_t limit) override;
    bool cancelRequest(int64_t user_id, int64_t request_id) override;
    domain::AccountTransactionPage listTransactions(int64_t user_id, int64_t offset,
                                                    int64_t limit) override;

private:
    std::shared_ptr<Db> db_;
};

} // namespace ecshop::infra
