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

    // POST /api/v1/me/bookings — register a backorder for visible goods
    sv.POST("/api/v1/me/bookings",
            [users, bookings](const wfrest::HttpReq *req, wfrest::HttpResp *resp)
    {
        api::ApiResponse err;
        std::optional<int64_t> user_id = api::authenticate(req, users, err);
        if (!user_id)
        {
            api::send(req, resp, err);
            return;
        }

        wfrest::Json body;
        if (!api::parseJsonBody(req, body, err))
        {
            api::send(req, resp, err);
            return;
        }

        domain::Booking booking;
        if (!api::readInt(body, "goods_id", booking.goods_id) || booking.goods_id <= 0)
        {
            wfrest::Json::Object details;
            details.push_back("field", "goods_id");
            api::send(req, resp,
                      ApiError::validationError("goods_id must be a positive integer", details));
            return;
        }
        if (!api::readInt(body, "goods_number", booking.quantity) || booking.quantity < 1 ||
            booking.quantity > 999)
        {
            wfrest::Json::Object details;
            details.push_back("field", "goods_number");
            api::send(req, resp,
                      ApiError::validationError("goods_number must be between 1 and 999", details));
            return;
        }

        struct StringField
        {
            const char *name;
            std::string *target;
            size_t max_len;
        } fields[] = {
            {"description", &booking.description, 255},
            {"linkman", &booking.linkman, 60},
            {"email", &booking.email, 120},
            {"telephone", &booking.telephone, 32},
        };
        for (const StringField &field : fields)
        {
            if (body.has(field.name) &&
                (!api::readStr(body, field.name, *field.target) ||
                 field.target->size() > field.max_len))
            {
                wfrest::Json::Object details;
                details.push_back("field", std::string(field.name));
                api::send(req, resp,
                          ApiError::validationError(std::string(field.name) + " is too long",
                                                    details));
                return;
            }
        }

        domain::BookingAddResult result = bookings->add(*user_id, booking);
        if (result == domain::BookingAddResult::GoodsNotVisible)
        {
            api::send(req, resp, ApiError::notFound("goods not found"));
            return;
        }
        if (result == domain::BookingAddResult::Duplicate)
        {
            api::send(req, resp, ApiError::conflict("booking_exists", "goods already booked"));
            return;
        }

        api::send(req, resp, ApiResponse::created(bookingToJson(booking)));
    });

    // DELETE /api/v1/me/bookings/{id}
    sv.DELETE("/api/v1/me/bookings/{id}",
              [users, bookings](const wfrest::HttpReq *req, wfrest::HttpResp *resp)
    {
        api::ApiResponse err;
        std::optional<int64_t> user_id = api::authenticate(req, users, err);
        if (!user_id)
        {
            api::send(req, resp, err);
            return;
        }

        int64_t rec_id = 0;
        if (!api::parsePathId(req, "id", rec_id))
        {
            wfrest::Json::Object details;
            details.push_back("field", "id");
            api::send(req, resp, ApiError::validationError("id must be a positive integer", details));
            return;
        }

        if (!bookings->remove(*user_id, rec_id))
        {
            api::send(req, resp, ApiError::notFound("booking not found"));
            return;
        }

        api::send(req, resp, ApiResponse::noContent());
    });
}

} // namespace ecshop::http
