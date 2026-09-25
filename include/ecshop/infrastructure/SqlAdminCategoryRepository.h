#pragma once

#include "ecshop/domain/Catalog.h"
#include "ecshop/infrastructure/db/Db.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace ecshop::infra {

class SqlAdminCategoryRepository
{
public:
    explicit SqlAdminCategoryRepository(std::shared_ptr<Db> db) : db_(std::move(db)) {}

    // all categories (is_show regardless), with goods_count
    std::vector<domain::CategorySummary> list();

    // returns new cat_id
    int64_t create(const std::string &name, int64_t parent_id, int64_t sort_order);

    // patch fields; false when missing
    bool patch(int64_t cat_id, const std::optional<std::string> &name,
               const std::optional<int64_t> &parent_id,
               const std::optional<int64_t> &sort_order, const std::optional<bool> &is_show);

    // false when the category has goods
    bool remove(int64_t cat_id);

private:
    std::shared_ptr<Db> db_;
};

} // namespace ecshop::infra
