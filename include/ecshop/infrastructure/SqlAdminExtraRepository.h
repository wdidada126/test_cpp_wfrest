#pragma once

#include "ecshop/domain/Account.h"
#include "ecshop/domain/Order.h"
#include "ecshop/domain/User.h"
#include "ecshop/infrastructure/db/Db.h"

#include <memory>
#include <optional>
#include <vector>

namespace ecshop::infra {

// admin modules shared across the remaining M5 URLs: promotion writes, user
// reads, account settlement, audit and metrics
class SqlAdminExtraRepository
{
public:
    explicit SqlAdminExtraRepository(std::shared_ptr<Db> db) : db_(std::move(db)) {}

    // users ------------------------------------------------------------------
    struct AdminUserRow
    {
        int64_t user_id = 0;
        std::string username;
        std::string email;
        bool verified = false;
        std::string created_at;
    };
    struct AdminUserRowPage
    {
        int64_t total = 0;
        std::vector<AdminUserRow> items;
    };

    AdminUserRowPage listUsers(int64_t offset, int64_t limit);
    bool updateUserEmail(int64_t user_id, const std::string &email);

    // goods_activity write ops (group_buy/auction/snatch)
    // act_type: 0=snatch 1=group_buy 2=auction
    int64_t createPromotion(const std::string &name, const std::string &description,
                            int64_t act_type, int64_t goods_id, int64_t start_time,
                            int64_t end_time, const std::string &ext_info);
    bool deletePromotion(int64_t act_id, int64_t act_type);

    // account request settlement: deposit -> paid (available += amount),
    // withdrawal -> disbursed (frozen -= amount)
    struct SettleRequest
    {
        int64_t rec_id = 0;
        std::string process_type;
        int64_t amount_cents = 0;
        std::string status;
    };
    std::optional<SettleRequest> findAccountRequest(int64_t rec_id);
    bool settleDeposit(int64_t rec_id, int64_t amount_cents);
    bool settleWithdrawal(int64_t rec_id, int64_t amount_cents);

    // metrics
    int64_t countUsers();
    int64_t countOrders();
    int64_t countGoodsAll();
    int64_t countRevenues(); // paid orders

private:
    std::shared_ptr<Db> db_;
};

} // namespace ecshop::infra
