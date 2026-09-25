#include "ecshop/shared/Money.h"

namespace ecshop::shared {

bool Money::parse(const std::string &text, int64_t &cents)
{
    if (text.empty())
        return false;

    size_t i = 0;
    bool negative = false;
    if (text[i] == '-')
    {
        negative = true;
        ++i;
    }

    int64_t whole = 0;
    bool saw_digit = false;
    for (; i < text.size() && text[i] != '.' && text[i] != ','; ++i)
    {
        if (text[i] < '0' || text[i] > '9')
            return false;
        saw_digit = true;
        whole = whole * 10 + (text[i] - '0');
    }

    int64_t frac = 0;
    int frac_digits = 0;
    if (i < text.size() && text[i] == '.')
    {
        ++i;
        for (; i < text.size(); ++i)
        {
            if (text[i] < '0' || text[i] > '9')
                return false;
            saw_digit = true;
            if (frac_digits < 2)
            {
                frac = frac * 10 + (text[i] - '0');
                ++frac_digits;
            }
        }
    }

    if (!saw_digit)
        return false;

    while (frac_digits < 2)
    {
        frac *= 10;
        ++frac_digits;
    }

    cents = whole * 100 + frac;
    if (negative)
        cents = -cents;
    return true;
}

std::string Money::format(int64_t cents)
{
    bool negative = cents < 0;
    uint64_t abs = negative ? static_cast<uint64_t>(-cents) : static_cast<uint64_t>(cents);
    uint64_t whole = abs / 100;
    uint64_t frac = abs % 100;

    std::string out;
    if (negative)
        out.push_back('-');
    out += std::to_string(whole);
    out.push_back('.');
    out.push_back(static_cast<char>('0' + frac / 10));
    out.push_back(static_cast<char>('0' + frac % 10));
    return out;
}

std::string Money::normalize(const std::string &text)
{
    int64_t cents = 0;
    if (!parse(text, cents))
        return "0.00";
    return format(cents);
}

} // namespace ecshop::shared
