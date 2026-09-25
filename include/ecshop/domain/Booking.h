#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace ecshop::domain {

struct Booking
{
    int64_t rec_id = 0;
    int64_t goods_id = 0;
    std::string goods_name;
    int64_t quantity = 0;
    std::string description;
    std::string linkman;
    std::string email;
    std::string telephone;
    std::string booking_time; // unix seconds as text
    bool disposed = false;
};

enum class BookingAddResult
{
    Ok,
    GoodsNotVisible,
    Duplicate,
};

class BookingRepository
{
public:
    virtual ~BookingRepository() = default;

    virtual std::vector<Booking> listOfUser(int64_t user_id) = 0;

    virtual BookingAddResult add(int64_t user_id, const Booking &booking) = 0;

    virtual bool remove(int64_t user_id, int64_t rec_id) = 0;
};

} // namespace ecshop::domain
