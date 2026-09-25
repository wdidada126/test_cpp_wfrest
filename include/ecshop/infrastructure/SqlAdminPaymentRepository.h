#pragma once

#include "ecshop/domain/Checkout.h"
#include "ecshop/infrastructure/db/Db.h"

#include <memory>
#include <optional>

namespace ecshop::infra {

// payment + shipping methods; admin perspective (all rows, enable toggling)
class SqlAdminPaymentRepository
{
public:
    explicit SqlAdminPaymentRepository(std::shared_ptr<Db> db) : db_(std::move(db)) {}

    std::vector<domain::PaymentOption> listAll();    // enabled and disabled
    std::optional<domain::PaymentOption> find(int64_t pay_id);
    void create(const std::string &name, const std::string &fee, bool enabled);
    bool patch(int64_t pay_id, const std::optional<std::string> &name,
               const std::optional<std::string> &fee, const std::optional<bool> &enabled);

private:
    std::shared_ptr<Db> db_;
};

class SqlAdminShippingRepository
{
public:
    explicit SqlAdminShippingRepository(std::shared_ptr<Db> db) : db_(std::move(db)) {}

    std::vector<domain::ShippingOption> listAll();
    std::optional<domain::ShippingOption> find(int64_t shipping_id);
    void create(const std::string &name, const std::string &fee, bool enabled);
    bool patch(int64_t shipping_id, const std::optional<std::string> &name,
               const std::optional<std::string> &fee, const std::optional<bool> &enabled);

private:
    std::shared_ptr<Db> db_;
};

} // namespace ecshop::infra
