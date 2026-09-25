#pragma once

#include <vector>

#include "ecshop/domain/Booking.h"
#include "ecshop/infrastructure/db/Db.h"

#include <memory>

namespace ecshop::infra {

class SqlBookingRepository : public domain::BookingRepository
{
public:
    explicit SqlBookingRepository(std::shared_ptr<Db> db) : db_(std::move(db)) {}

    std::vector<domain::Booking> listOfUser(int64_t user_id) override;
    domain::BookingAddResult add(int64_t user_id, const domain::Booking &booking) override;
    bool remove(int64_t user_id, int64_t rec_id) override;

private:
    std::shared_ptr<Db> db_;
};

} // namespace ecshop::infra
