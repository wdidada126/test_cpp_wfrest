#pragma once

#include "ecshop/domain/Favorite.h"
#include "ecshop/infrastructure/db/Db.h"

#include <memory>

namespace ecshop::infra {

class SqlFavoriteRepository : public domain::FavoriteRepository
{
public:
    explicit SqlFavoriteRepository(std::shared_ptr<Db> db) : db_(std::move(db)) {}

    std::vector<domain::Favorite> listOfUser(int64_t user_id) override;
    domain::FavoriteAddResult add(int64_t user_id, int64_t goods_id) override;
    bool setAttention(int64_t user_id, int64_t rec_id, bool attention) override;
    bool remove(int64_t user_id, int64_t rec_id) override;

private:
    std::shared_ptr<Db> db_;
};

} // namespace ecshop::infra
