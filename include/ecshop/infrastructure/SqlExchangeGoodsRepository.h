#pragma once

#include <optional>

#include "ecshop/domain/ExchangeGoodsRepository.h"
#include "ecshop/infrastructure/db/Db.h"

#include <memory>

namespace ecshop::infra {

class SqlExchangeGoodsRepository : public domain::ExchangeGoodsRepository
{
public:
    explicit SqlExchangeGoodsRepository(std::shared_ptr<Db> db) : db_(std::move(db)) {}

    domain::ExchangeGoodsPage list(const domain::ExchangeGoodsRepository::Query &query,
                                   int64_t offset, int64_t limit) override;
    std::optional<domain::ExchangeGoodsRow> findEnabled(int64_t goods_id) override;

private:
    std::shared_ptr<Db> db_;
};

} // namespace ecshop::infra
