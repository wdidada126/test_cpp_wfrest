#pragma once

#include "ecshop/domain/AdminGoodsRepository.h"
#include "ecshop/infrastructure/db/Db.h"

#include <memory>

namespace ecshop::infra {

class SqlAdminGoodsRepository : public domain::AdminGoodsRepository
{
public:
    explicit SqlAdminGoodsRepository(std::shared_ptr<Db> db) : db_(std::move(db)) {}

    domain::AdminGoodsPage list(const std::string &q, int64_t offset, int64_t limit) override;
    std::optional<domain::AdminGoodsRow> find(int64_t goods_id) override;
    int64_t create(const domain::AdminGoodsRow &goods, const std::string &description) override;
    bool patch(int64_t goods_id, const domain::AdminGoodsPatch &patch) override;
    bool softDelete(int64_t goods_id) override;

private:
    std::shared_ptr<Db> db_;
};

} // namespace ecshop::infra
