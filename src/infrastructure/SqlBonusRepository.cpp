#include "ecshop/infrastructure/SqlBonusRepository.h"
#include "ecshop/shared/Money.h"

namespace ecshop::infra {

using domain::BonusRecord;

static BonusRecord rowToBonus(const Row &row)
{
    BonusRecord bonus;
    bonus.bonus_id = row.getInt("bonus_id");
    bonus.bonus_type_id = row.getInt("bonus_type_id");
    bonus.bonus_sn = row.get("bonus_sn");
    bonus.money = shared::Money::normalize(row.get("type_money"));
    bonus.use_start_date = row.getInt("use_start_date");
    bonus.use_end_date = row.getInt("use_end_date");
    bonus.user_id = row.getInt("user_id");
    bonus.used_time = row.getInt("used_time");
    bonus.order_id = row.getInt("order_id");
    return bonus;
}

std::optional<BonusRecord> SqlBonusRepository::findBySn(const std::string &bonus_sn)
{
    std::string sql =
        "SELECT ub.bonus_id AS bonus_id, ub.bonus_type_id AS bonus_type_id,"
        " ub.bonus_sn AS bonus_sn, ub.user_id AS user_id, ub.used_time AS used_time,"
        " ub.order_id AS order_id, bt.type_money AS type_money,"
        " bt.use_start_date AS use_start_date, bt.use_end_date AS use_end_date"
        " FROM " + db_->table("user_bonus") + " ub JOIN " + db_->table("bonus_type") +
        " bt ON bt.type_id = ub.bonus_type_id WHERE ub.bonus_sn = ?";

    std::vector<Row> rows = db_->query(sql, {bonus_sn});
    if (rows.empty())
        return std::nullopt;
    return rowToBonus(rows.front());
}

bool SqlBonusRepository::claim(int64_t bonus_id, int64_t user_id)
{
    std::string sql = "UPDATE " + db_->table("user_bonus") +
                      " SET user_id = ? WHERE bonus_id = ? AND user_id = 0";
    return db_->execute(sql, {std::to_string(user_id), std::to_string(bonus_id)}) > 0;
}

std::vector<BonusRecord> SqlBonusRepository::listOfUser(int64_t user_id)
{
    std::string sql =
        "SELECT ub.bonus_id AS bonus_id, ub.bonus_type_id AS bonus_type_id,"
        " ub.bonus_sn AS bonus_sn, ub.user_id AS user_id, ub.used_time AS used_time,"
        " ub.order_id AS order_id, bt.type_money AS type_money,"
        " bt.use_start_date AS use_start_date, bt.use_end_date AS use_end_date"
        " FROM " + db_->table("user_bonus") + " ub JOIN " + db_->table("bonus_type") +
        " bt ON bt.type_id = ub.bonus_type_id WHERE ub.user_id = ?"
        " ORDER BY ub.bonus_id DESC";

    std::vector<BonusRecord> items;
    for (const Row &row : db_->query(sql, {std::to_string(user_id)}))
        items.push_back(rowToBonus(row));
    return items;
}

} // namespace ecshop::infra
