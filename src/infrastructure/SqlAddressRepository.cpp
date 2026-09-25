#include "ecshop/infrastructure/SqlAddressRepository.h"

namespace ecshop::infra {

using domain::Address;

static Address rowToAddress(const Row &row)
{
    Address address;
    address.address_id = row.getInt("address_id");
    address.consignee = row.get("consignee");
    address.country_id = row.getInt("country_id");
    address.province_id = row.getInt("province_id");
    address.city_id = row.getInt("city_id");
    address.district_id = row.getInt("district_id");
    address.address = row.get("address");
    address.zip = row.get("zipcode");
    address.mobile = row.get("mobile");
    address.is_default = row.getInt("is_default") != 0;
    return address;
}

std::vector<Address> SqlAddressRepository::listOfUser(int64_t user_id)
{
    std::string sql =
        "SELECT address_id AS address_id, consignee AS consignee, country_id AS country_id,"
        " province_id AS province_id, city_id AS city_id, district_id AS district_id,"
        " address AS address, zipcode AS zipcode, mobile AS mobile, is_default AS is_default"
        " FROM " + db_->table("user_address") +
        " WHERE user_id = ? ORDER BY is_default DESC, address_id DESC";

    std::vector<Address> items;
    for (const Row &row : db_->query(sql, {std::to_string(user_id)}))
        items.push_back(rowToAddress(row));
    return items;
}

int64_t SqlAddressRepository::create(int64_t user_id, const Address &address)
{
    std::string address_t = db_->table("user_address");
    int64_t address_id = 0;

    db_->transaction([&] {
        if (address.is_default)
            db_->execute("UPDATE " + address_t + " SET is_default = 0 WHERE user_id = ?",
                         {std::to_string(user_id)});

        db_->execute("INSERT INTO " + address_t +
                         " (user_id, consignee, country_id, province_id, city_id, district_id,"
                         " address, zipcode, mobile, is_default)"
                         " VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)",
                     {std::to_string(user_id), address.consignee,
                      std::to_string(address.country_id), std::to_string(address.province_id),
                      std::to_string(address.city_id), std::to_string(address.district_id),
                      address.address, address.zip, address.mobile,
                      address.is_default ? "1" : "0"});
        address_id = db_->lastInsertId();
    });
    return address_id;
}

bool SqlAddressRepository::update(int64_t user_id, int64_t address_id, const Address &address)
{
    std::string address_t = db_->table("user_address");

    bool ok = false;
    db_->transaction([&] {
        if (address.is_default)
            db_->execute("UPDATE " + address_t + " SET is_default = 0 WHERE user_id = ?",
                         {std::to_string(user_id)});

        ok = db_->execute(
                 "UPDATE " + address_t +
                     " SET consignee = ?, country_id = ?, province_id = ?, city_id = ?,"
                     " district_id = ?, address = ?, zipcode = ?, mobile = ?, is_default = ?"
                     " WHERE address_id = ? AND user_id = ?",
                 {address.consignee, std::to_string(address.country_id),
                  std::to_string(address.province_id), std::to_string(address.city_id),
                  std::to_string(address.district_id), address.address, address.zip,
                  address.mobile, address.is_default ? "1" : "0",
                  std::to_string(address_id), std::to_string(user_id)}) > 0;
    });
    return ok;
}

bool SqlAddressRepository::remove(int64_t user_id, int64_t address_id)
{
    std::string sql = "DELETE FROM " + db_->table("user_address") +
                      " WHERE address_id = ? AND user_id = ?";
    return db_->execute(sql, {std::to_string(address_id), std::to_string(user_id)}) > 0;
}

} // namespace ecshop::infra
