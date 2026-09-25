#pragma once

#include "ecshop/domain/PaymentRepository.h"
#include "ecshop/infrastructure/db/Db.h"

namespace ecshop::infra {

class SqlPaymentRepository : public domain::PaymentRepository
{
public:
    explicit SqlPaymentRepository(std::shared_ptr<Db> db) : db_(std::move(db)) {}

    domain::PaymentCallbackStatus applyCallback(const std::string &provider,
                                                const std::string &provider_trade_no,
                                                const std::string &order_sn,
                                                int64_t amount_cents,
                                                const std::string &raw_payload) override;

private:
    std::shared_ptr<Db> db_;
};

} // namespace ecshop::infra
