#pragma once

#include <cstdint>
#include <string>

namespace ecshop::shared {

// unix seconds (UTC) -> "2026-01-31T12:00:00Z"; 0 -> empty string
std::string isoUtc(int64_t unix_seconds);

int64_t nowUnix();

} // namespace ecshop::shared
