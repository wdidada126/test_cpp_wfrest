#include "ecshop/infrastructure/SqlBookingRepository.h"

namespace ecshop::infra {

using domain::Booking;
using domain::BookingAddResult;

static Booking rowToBooking(const Row &row)
{
    Booking booking;
    booking.rec_id = row.getInt("rec_id");
    booking.goods_id = row.getInt("goods_id");
    booking.goods_name = row.get("goods_name");
    booking.quantity = row.getInt("goods_number");
    booking.description = row.get("goods_desc");
    booking.linkman = row.get("link_man");
    booking.email = row.get("email");
    booking.telephone = row.get("tel");
    booking.booking_time = row.get("booking_time");
    booking.disposed = row.getInt("is_dispose") != 0;
    return booking;
}

std::vector<Booking> SqlBookingRepository::listOfUser(int64_t user_id)
{
    std::string sql =
        "SELECT bg.rec_id AS rec_id, bg.goods_id AS goods_id, g.goods_name AS goods_name,"
        " bg.goods_number AS goods_number, bg.goods_desc AS goods_desc,"
        " bg.link_man AS link_man, bg.email AS email, bg.tel AS tel, bg.is_dispose AS is_dispose," +
        db_->toUnix("bg.booking_time") + " AS booking_time FROM " + db_->table("booking_goods") +
        " bg LEFT JOIN " + db_->table("goods") + " g ON g.goods_id = bg.goods_id"
        " WHERE bg.user_id = ? ORDER BY bg.booking_time DESC, bg.rec_id DESC";

    std::vector<Booking> items;
    for (const Row &row : db_->query(sql, {std::to_string(user_id)}))
        items.push_back(rowToBooking(row));
    return items;
}

BookingAddResult SqlBookingRepository::add(int64_t user_id, const Booking &booking)
{
    BookingAddResult result = BookingAddResult::Ok;

    db_->transaction([&] {
        std::vector<Row> goods = db_->query(
            "SELECT goods_id AS goods_id FROM " + db_->table("goods") +
                " WHERE goods_id = ? AND is_on_sale = 1 AND is_delete = 0",
            {std::to_string(booking.goods_id)});
        if (goods.empty())
        {
            result = BookingAddResult::GoodsNotVisible;
            return;
        }

        // single INSERT with saleable EXISTS; unique(user_id, goods_id) dedupes
        try
        {
            std::string sql =
                "INSERT INTO " + db_->table("booking_goods") +
                " (user_id, email, link_man, tel, goods_id, goods_desc, goods_number)"
                " SELECT ?, ?, ?, ?, ?, ?, ? WHERE EXISTS (SELECT 1 FROM " +
                db_->table("goods") +
                " WHERE goods_id = ? AND is_on_sale = 1 AND is_delete = 0)";
            db_->execute(sql, {std::to_string(user_id), booking.email, booking.linkman,
                               booking.telephone, std::to_string(booking.goods_id),
                               booking.description, std::to_string(booking.quantity),
                               std::to_string(booking.goods_id)});
        }
        catch (const DbError &)
        {
            // unique(user_id, goods_id) race
            result = BookingAddResult::Duplicate;
        }
    });
    return result;
}

bool SqlBookingRepository::remove(int64_t user_id, int64_t rec_id)
{
    std::string sql = "DELETE FROM " + db_->table("booking_goods") +
                      " WHERE rec_id = ? AND user_id = ?";
    return db_->execute(sql, {std::to_string(rec_id), std::to_string(user_id)}) > 0;
}

} // namespace ecshop::infra
