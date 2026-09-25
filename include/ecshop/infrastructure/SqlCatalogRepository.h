#pragma once

#include "ecshop/domain/GoodsRepository.h"
#include "ecshop/infrastructure/db/Db.h"

namespace ecshop::infra {

// SQL-backed catalog reads. One implementation serves both SQLite and MySQL
// through the dialect helpers on Db (table names, gallery columns, time).
class SqlCatalogRepository : public domain::GoodsRepository
{
public:
    explicit SqlCatalogRepository(std::shared_ptr<Db> db) : db_(std::move(db)) {}

    std::optional<domain::Goods> findVisibleGoods(int64_t goods_id) override;

private:
    std::shared_ptr<Db> db_;
};

} // namespace ecshop::infra
