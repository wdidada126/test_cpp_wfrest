#include "ecshop/infrastructure/SqlRegionRepository.h"

namespace ecshop::infra {

domain::RegionPage SqlRegionRepository::listRegions(int64_t parent_id, int64_t region_type,
                                                    int64_t offset, int64_t limit)
{
    const std::string region_t = db_->table("region");

    std::string where = "1 = 1";
    Params params;
    if (parent_id > 0)
    {
        where += " AND parent_id = ?";
        params.push_back(std::to_string(parent_id));
    }
    if (region_type > 0)
    {
        where += " AND region_type = ?";
        params.push_back(std::to_string(region_type));
    }

    domain::RegionPage page;
    std::string count_sql = "SELECT COUNT(*) AS total FROM " + region_t + " WHERE " + where;
    std::vector<Row> counts = db_->query(count_sql, params);
    if (!counts.empty())
        page.total = counts.front().getInt("total");

    // limit/offset are validated integers formatted by us; user data stays bound.
    std::string sql =
        "SELECT region_id AS region_id, parent_id AS parent_id,"
        " region_name AS region_name, region_type AS region_type FROM " + region_t +
        " WHERE " + where + " ORDER BY region_type, region_id" +
        " LIMIT " + std::to_string(limit) + " OFFSET " + std::to_string(offset);

    for (const Row &row : db_->query(sql, params))
    {
        domain::Region item;
        item.region_id = row.getInt("region_id");
        item.parent_id = row.getInt("parent_id");
        item.name = row.get("region_name");
        item.region_type = row.getInt("region_type");
        page.items.push_back(std::move(item));
    }
    return page;
}

std::optional<domain::Region> SqlRegionRepository::find(int64_t region_id)
{
    std::string sql =
        "SELECT region_id AS region_id, parent_id AS parent_id,"
        " region_name AS region_name, region_type AS region_type FROM " + db_->table("region") +
        " WHERE region_id = ?";
    std::vector<Row> rows = db_->query(sql, {std::to_string(region_id)});
    if (rows.empty())
        return std::nullopt;

    domain::Region item;
    item.region_id = rows.front().getInt("region_id");
    item.parent_id = rows.front().getInt("parent_id");
    item.name = rows.front().get("region_name");
    item.region_type = rows.front().getInt("region_type");
    return item;
}

std::vector<domain::Region> SqlRegionRepository::listChildren(int64_t parent_id)
{
    std::string sql =
        "SELECT region_id AS region_id, parent_id AS parent_id,"
        " region_name AS region_name, region_type AS region_type FROM " + db_->table("region") +
        " WHERE parent_id = ? ORDER BY region_id";

    std::vector<domain::Region> items;
    for (const Row &row : db_->query(sql, {std::to_string(parent_id)}))
    {
        domain::Region item;
        item.region_id = row.getInt("region_id");
        item.parent_id = row.getInt("parent_id");
        item.name = row.get("region_name");
        item.region_type = row.getInt("region_type");
        items.push_back(std::move(item));
    }
    return items;
}

} // namespace ecshop::infra
