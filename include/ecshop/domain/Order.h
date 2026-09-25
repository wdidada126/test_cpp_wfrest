#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace ecshop::domain {

struct OrderGoods
{
    int64_t goods_id = 0;
    std::string name;
    std::string price; // snapshot, decimal string
    int64_t quantity = 0;
};

struct OrderSummary
{
    int64_t order_id = 0;
    std::string order_sn;
    std::string status; // pending_payment | paid | shipped | received | cancelled
    std::string goods_amount;
    std::string shipping_fee;
    std::string payment_fee;
    std::string order_amount;
    std::string created_at; // unix seconds as text
    bool replayed = false;
};

struct OrderDetail : OrderSummary
{
    std::string consignee;
    std::string address;
    std::string mobile;
    int64_t shipping_id = 0;
    int64_t payment_id = 0;
    std::string remark;
    std::vector<OrderGoods> items;
};

struct PlaceOrderCommand
{
    int64_t user_id = 0;
    std::string idempotency_key;
    std::string fingerprint; // canonical hash of the request body
    int64_t address_id = 0;
    int64_t shipping_id = 0;
    int64_t payment_id = 0;
    std::string remark;
};

enum class PlaceOrderStatus
{
    Created,
    Replayed,
    KeyMismatch,
    OutOfStock,
    EmptyCart,
};

enum class OrderCancelStatus
{
    Cancelled,
    NotFound,
    InvalidState,
};

struct OrderPage
{
    int64_t total = 0;
    std::vector<OrderSummary> items;
};

class OrderRepository
{
public:
    virtual ~OrderRepository() = default;

    // Single transaction: re-reads the cart, conditionally deducts stock,
    // writes order + order_goods snapshot and clears the cart.
    virtual PlaceOrderStatus placeOrder(const PlaceOrderCommand &command,
                                        OrderSummary &out) = 0;

    virtual OrderPage listOfUser(int64_t user_id, int64_t offset, int64_t limit) = 0;

    // nullopt when the order does not belong to this user
    virtual std::optional<OrderDetail> findOfUser(int64_t user_id, int64_t order_id) = 0;

    // cancel a pending_payment order: restocks goods, refunds used balance and
    // writes order/account audits in one transaction
    virtual OrderCancelStatus cancelOfUser(int64_t user_id, int64_t order_id,
                                           OrderSummary &out) = 0;
};

} // namespace ecshop::domain
