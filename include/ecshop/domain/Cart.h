#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace ecshop::domain {

struct CartItem
{
    int64_t id = 0;
    int64_t goods_id = 0;
    std::string name;
    std::string price; // decimal string, current shop price
    int64_t quantity = 0;
    int64_t version = 0;
};

enum class CartAddResult
{
    Ok,
    GoodsNotVisible,
};

class CartRepository
{
public:
    virtual ~CartRepository() = default;

    virtual std::vector<CartItem> listOfUser(int64_t user_id) = 0;

    // add or atomically accumulate quantity and bump the version
    virtual CartAddResult add(int64_t user_id, int64_t goods_id, int64_t quantity,
                              CartItem &out) = 0;

    // optimistic lock on version; nullopt when missing/not owned, version=false
    // when stale
    virtual std::optional<bool> updateQuantity(int64_t user_id, int64_t rec_id,
                                               int64_t quantity, int64_t version) = 0;

    virtual bool remove(int64_t user_id, int64_t rec_id) = 0;
};

} // namespace ecshop::domain
