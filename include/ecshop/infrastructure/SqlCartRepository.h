#pragma once

#include "ecshop/domain/Cart.h"
#include "ecshop/infrastructure/db/Db.h"

#include <memory>

namespace ecshop::infra {

class SqlCartRepository : public domain::CartRepository
{
public:
    explicit SqlCartRepository(std::shared_ptr<Db> db) : db_(std::move(db)) {}

    std::vector<domain::CartItem> listOfUser(int64_t user_id) override;
    domain::CartAddResult add(int64_t user_id, int64_t goods_id, int64_t quantity,
                              domain::CartItem &out) override;
    std::optional<bool> updateQuantity(int64_t user_id, int64_t rec_id, int64_t quantity,
                                       int64_t version) override;
    bool remove(int64_t user_id, int64_t rec_id) override;

private:
    std::optional<domain::CartItem> findByGoods(int64_t user_id, int64_t goods_id);
    std::shared_ptr<Db> db_;
};

} // namespace ecshop::infra
