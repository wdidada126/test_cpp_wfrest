#include "ecshop/http/BookingRoutes.h"
#include "ecshop/http/AuthUtil.h"
#include "ecshop/http/HttpUtil.h"
#include "ecshop/infrastructure/SqlBookingRepository.h"
#include "ecshop/infrastructure/SqlUserRepository.h"
#include "ecshop/shared/TimeUtil.h"

#include <cstdlib>
#include <optional>
#include <vector>

namespace ecshop::http {

using api::ApiError;
using api::ApiResponse;

static wfrest::Json bookingToJson(const domain::Booking &booking)
{
    wfrest::Json::Object obj;
    obj.push_back("id", booking.rec_id);
    obj.push_back("goods_id", booking.goods_id);
    obj.push_back("name", booking.goods_name);
    obj.push_back("quantity", booking.quantity);
    obj.push_back("description", booking.description);
    obj.push_back("linkman", booking.linkman);
    obj.push_back("email", booking.email);
    obj.push_back("telephone", booking.telephone);
    obj.push_back("booking_time",
                  shared::isoUtc(std::strtoll(booking.booking_time.c_str(), nullptr, 10)));
    obj.push_back("disposed", booking.disposed);
    return obj;
}

void registerBookingRoutes(wfrest::HttpServer &sv, std::shared_ptr<infra::Db> db)
{
    auto users = std::make_shared<infra::SqlUserRepository>(db);
    auto bookings = std::make_shared<infra::SqlBookingRepository>(db);

    // GET /api/v1/me/bookings — own backorder registrations
    sv.GET("/api/v1/me/bookings",
           [users, bookings](const wfrest::HttpReq *req, wfrest::HttpResp *resp)
    {
        api::ApiResponse err;
        std::optional<int64_t> user_id = api::authenticate(req, users, err);
        if (!user_id)
        {
            api::send(req, resp, err);
            return;
        }

        std::vector<domain::Booking> all = bookings->listOfUser(*user_id);

        wfrest::Json::Array items;
        for (const domain::Booking &booking : all)
            items.push_back(bookingToJson(booking));

        api::PageQuery page;
        api::send(req, resp,
                  ApiResponse::ok(api::listBody(page, static_cast<int64_t>(all.size()), items)));
    });
}

} // namespace ecshop::http
