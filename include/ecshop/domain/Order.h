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

enum class OrderReturnStatus
{
    Ok,
    NotFound,
    NothingToReturn,
};

enum class OrderPatchStatus
{
    Ok,
    NotFound,
    InvalidState,
};

// PATCH /me/orders/{id}/address fields (docs/03)
struct DeliveryAddressPatch
{
    std::string consignee;
    std::string email;
    std::string address;
    std::string zipcode;
    std::string tel;
    std::string mobile;
    std::string sign_building;
    std::string best_time;
};

// surplus patch result
enum class OrderSurplusStatus
{
    PartiallyPaid, // amount accepted, order still pending_payment
    FullyPaid,     // order flipped to paid
    NotFound,
    InvalidState,
};

struct SurplusResult
{
    OrderSurplusStatus status = OrderSurplusStatus::NotFound;
    std::string applied;    // actually applied amount (after truncation)
    std::string remaining;  // remaining payable (without payment fee)
    std::string paid_total; // accumulated order_balance_payment.paid_cents
    bool became_paid = false;
};

enum class OrderMergeStatus
{
    Merged,       // 201, new order created
    NotFound,     // one of the orders missing/not owned
    InvalidState, // same ids / not pending / balance used
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

    // confirm receipt: paid -> received with an audit row
    virtual OrderCancelStatus receivedOfUser(int64_t user_id, int64_t order_id,
                                             OrderSummary &out) = 0;

    // merge the order's goods snapshot back into the user's cart, capped at the
    // current saleable stock
    virtual OrderReturnStatus returnToCart(int64_t user_id, int64_t order_id) = 0;

    // status query by order number, always scoped to the current user
    virtual std::optional<OrderSummary> findBySnOfUser(int64_t user_id,
                                                       const std::string &order_sn) = 0;

    // PATCH /me/orders/{id}/address — pending_payment + unshipped only
    virtual OrderPatchStatus updateAddressOfUser(int64_t user_id, int64_t order_id,
                                                 const DeliveryAddressPatch &patch) = 0;

    // PATCH /me/orders/{id}/payment — recompute fees; 400 on same method
    virtual OrderPatchStatus updatePaymentOfUser(int64_t user_id, int64_t order_id,
                                                 int64_t payment_id) = 0;

    // PATCH /me/orders/{id}/surplus — pay with account balance; amount is
    // truncated to the remaining payable (without payment fee)
    virtual SurplusResult payWithSurplus(int64_t user_id, int64_t order_id,
                                         int64_t amount_cents) = 0;

    // POST /me/orders/merge — merge two own pending_payment orders without
    // balance usage into one new order
    virtual OrderMergeStatus mergeOrders(int64_t user_id, int64_t from_order_id,
                                         int64_t to_order_id, OrderSummary &out) = 0;
};

} // namespace ecshop::domain
