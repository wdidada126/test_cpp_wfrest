#pragma once

#include <cstdint>
#include <string>

namespace ecshop::shared {

// Money is kept as integer cents; wire format is a decimal string like "99.90".
// Never use double.
class Money
{
public:
    // "99.90" -> 9990, "49.9" -> 4990, "8" -> 800. Returns false on garbage.
    static bool parse(const std::string &text, int64_t &cents);

    // 9990 -> "99.90"
    static std::string format(int64_t cents);

    // normalize any accepted decimal text to two-fractional-digit form.
    // empty/garbage becomes "0.00".
    static std::string normalize(const std::string &text);
};

} // namespace ecshop::shared
