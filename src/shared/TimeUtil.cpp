#include "ecshop/shared/TimeUtil.h"

#include <cstdio>
#include <ctime>

namespace ecshop::shared {

std::string isoUtc(int64_t unix_seconds)
{
    if (unix_seconds <= 0)
        return {};

    time_t t = static_cast<time_t>(unix_seconds);
    struct tm tm_utc {};
#if defined(_WIN32)
    gmtime_s(&tm_utc, &t);
#else
    gmtime_r(&t, &tm_utc);
#endif

    char buf[48];
    std::snprintf(buf, sizeof(buf), "%04d-%02d-%02dT%02d:%02d:%02dZ",
                  tm_utc.tm_year + 1900, tm_utc.tm_mon + 1, tm_utc.tm_mday,
                  tm_utc.tm_hour, tm_utc.tm_min, tm_utc.tm_sec);
    return std::string(buf);
}

int64_t nowUnix()
{
    return static_cast<int64_t>(std::time(nullptr));
}

} // namespace ecshop::shared
