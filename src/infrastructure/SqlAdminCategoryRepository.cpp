#include "ecshop/infrastructure/SqlAdminCategoryRepository.h"

namespace ecshop::infra {

std::vector<domain::CategorySummary> SqlAdminCategoryRepository::list()
{
    const std::string category_t = db_->table("category");
    const std::string goods_t = db_->table("goods");

    std::string sql =
        "SELECT c.cat_id AS cat_id, c.parent_id AS parent_id, c.cat_name AS cat_name,"
        " (SELECT COUNT(*) FROM " + goods_t + " g WHERE g.cat_id = c.cat_id) AS goods_count"
        " FROM " + category_t + " c ORDER BY c.parent_id, c.sort_order, c.cat_id";

    std::vector<domain::CategorySummary> items;
    for (const Row &row : db_->query(sql, {}))
    {
        domain::CategorySummary item;
        item.cat_id = row.getInt("cat_id");
        item.parent_id = row.getInt("parent_id");
        item.name = row.get("cat_name");
        item.goods_count = row.getInt("goods_count");
        items.push_back(std::move(item));
    }
    return items;
}

int64_t SqlAdminCategoryRepository::create(const std::string &name, int64_t parent_id,
                                           int64_t sort_order)
{
    std::string sql = "INSERT INTO " + db_->table("category") +
                      " (parent_id, cat_name, sort_order) VALUES (?, ?, ?)";
    db_->execute(sql, {std::to_string(parent_id), name, std::to_string(sort_order)});
    return db_->lastInsertId();
}

bool SqlAdminCategoryRepository::patch(int64_t cat_id, const std::optional<std::string> &name,
                                       const std::optional<int64_t> &parent_id,
                                       const std::optional<int64_t> &sort_order,
                                       const std::optional<bool> &is_show)
{
    std::string set_sql;
    Params params;
    auto append = [&set_sql, &params](const std::string &column, const std::string &value) {
        if (!set_sql.empty())
            set_sql += ", ";
        set_sql += column + " = ?";
        params.push_back(value);
    };

    if (name)
        append("cat_name", *name);
    if (parent_id)
        append("parent_id", std::to_string(*parent_id));
    if (sort_order)
        append("sort_order", std::to_string(*sort_order));
    if (is_show)
        append("is_show", *is_show ? "1" : "0");

    if (set_sql.empty())
        return false;

    return db_->execute("UPDATE " + db_->table("category") + " SET " + set_sql +
                            " WHERE cat_id = ?",
                        params) > 0;
}

bool SqlAdminCategoryRepository::remove(int64_t cat_id)
{
    const std::string goods_t = db_->table("goods");

    std::vector<Row> used = db_->query(
        "SELECT goods_id AS goods_id FROM " + goods_t +
            " WHERE cat_id = ? LIMIT 1",
        {std::to_string(cat_id)});
    if (!used.empty())
        return false;

    return db_->execute("DELETE FROM " + db_->table("category") + " WHERE cat_id = ?",
                        {std::to_string(cat_id)}) > 0;
}

} // namespace ecshop::infra
