#include "ecshop/infrastructure/SqlPackageRepository.h"
#include "ecshop/shared/Money.h"

#include <algorithm>

namespace ecshop::infra {

std::vector<domain::Package> SqlPackageRepository::listActive(int64_t limit)
{
    const std::string activity_t = db_->table("goods_activity");
    const std::string package_goods_t = db_->table("package_goods");
    const std::string goods_t = db_->table("goods");

    // active packages in the current time window with visible goods
    std::string sql =
        "SELECT act_id AS act_id, act_name AS act_name, act_desc AS act_desc,"
        " goods_number AS goods_number, start_time AS start_time, end_time AS end_time,"
        " ext_info AS ext_info FROM " + activity_t +
        " WHERE act_type = 4 AND is_finished = 0 AND start_time <= " + db_->unixNow() +
        " AND end_time >= " + db_->unixNow() +
        " ORDER BY act_id DESC LIMIT " + std::to_string(limit);

    std::vector<domain::Package> packages;
    for (const Row &row : db_->query(sql, {}))
    {
        domain::Package package;
        package.act_id = row.getInt("act_id");
        package.name = row.get("act_name");
        package.description = row.get("act_desc");
        package.goods_number = row.getInt("goods_number");
        package.start_time = shared::isoUtc(row.getInt("start_time"));
        package.end_time = shared::isoUtc(row.getInt("end_time"));

        // package price from the ext_info JSON "package_price" field
        int64_t package_cents = 0;
        std::string ext = row.get("ext_info");
        std::string needle = "\"package_price\"";
        size_t at = ext.find(needle);
        if (at != std::string::npos)
        {
            size_t colon = ext.find(':', at + needle.size());
            if (colon != std::string::npos)
            {
                size_t first = ext.find('"', colon);
                size_t last = first == std::string::npos
                                  ? std::string::npos
                                  : ext.find('"', first + 1);
                if (first != std::string::npos && last != std::string::npos)
                    shared::Money::parse(ext.substr(first + 1, last - first - 1),
                                         package_cents);
            }
        }

        // items with current prices
        std::string items_sql =
            "SELECT pg.goods_id AS goods_id, pg.goods_number AS goods_number,"
            " g.goods_name AS goods_name, g.shop_price AS shop_price FROM " + package_goods_t +
            " pg JOIN " + goods_t + " g ON g.goods_id = pg.goods_id"
            " WHERE pg.package_id = ? AND g.is_on_sale = 1 AND g.is_delete = 0"
            " ORDER BY pg.goods_id";
        int64_t subtotal_cents = 0;
        for (const Row &item_row : db_->query(items_sql, {std::to_string(package.act_id)}))
        {
            domain::PackageItem item;
            item.goods_id = item_row.getInt("goods_id");
            item.name = item_row.get("goods_name");
            item.quantity = item_row.getInt("goods_number");
            item.price = shared::Money::normalize(item_row.get("shop_price"));

            int64_t unit = 0;
            shared::Money::parse(item.price, unit);
            subtotal_cents += unit * item.quantity;
            package.items.push_back(std::move(item));
        }

        package.subtotal = shared::Money::format(subtotal_cents);
        package.package_price = shared::Money::format(package_cents);
        int64_t saving = subtotal_cents - package_cents;
        package.saving = shared::Money::format(saving > 0 ? saving : 0);
        packages.push_back(std::move(package));
    }
    return packages;
}

} // namespace ecshop::infra
