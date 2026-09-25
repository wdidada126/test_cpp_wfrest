#pragma once

#include <cstdint>
#include <string>

namespace ecshop::domain {

struct User
{
    int64_t user_id = 0;
    std::string username;
    std::string email;
    std::string created_at; // unix seconds as text
};

} // namespace ecshop::domain
