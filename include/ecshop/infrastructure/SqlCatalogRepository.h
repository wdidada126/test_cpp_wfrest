#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

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
    bool categoryVisible(int64_t cat_id) override;
    domain::GoodsPage listCategoryGoods(int64_t cat_id, int64_t offset, int64_t limit) override;
    domain::GoodsPage searchGoods(const domain::GoodsFilter &filter, int64_t offset,
                                  int64_t limit) override;
    std::vector<domain::GoodsSummary> listNewestGoods(int64_t limit) override;
    std::vector<domain::CategorySummary> listVisibleCategories() override;
    domain::BrandPage listVisibleBrands(int64_t offset, int64_t limit) override;
    bool brandVisible(int64_t brand_id) override;
    domain::GoodsPage listBrandGoods(int64_t brand_id, int64_t offset, int64_t limit) override;
    std::optional<domain::GoodsGallery> findGallery(int64_t goods_id) override;
    domain::QuotationPage listQuotation(const domain::GoodsFilter &filter, int64_t offset,
                                        int64_t limit) override;
    std::vector<domain::GoodsSummary> listByIntroType(const std::string &intro_type,
                                                      int64_t limit) override;
    std::vector<domain::GoodsSummary> listPublicGoods(const domain::GoodsFilter &filter,
                                                      int64_t limit) override;

private:
    domain::GoodsPage listGoods(const std::string &where_sql,
                                const infra::Params &params,
                                int64_t offset, int64_t limit);
    std::shared_ptr<Db> db_;
};

} // namespace ecshop::infra
