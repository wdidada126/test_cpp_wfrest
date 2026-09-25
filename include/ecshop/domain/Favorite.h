#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace ecshop::domain {

struct Favorite
{
    int64_t rec_id = 0;
    int64_t goods_id = 0;
    std::string name;
    std::string price; // decimal string
    bool attention = false;
    std::string add_time; // unix seconds as text
};

enum class FavoriteAddResult
{
    Added,
    Duplicate,
    GoodsNotVisible,
};

class FavoriteRepository
{
public:
    virtual ~FavoriteRepository() = default;

    virtual std::vector<Favorite> listOfUser(int64_t user_id) = 0;

    virtual FavoriteAddResult add(int64_t user_id, int64_t goods_id) = 0;

    // owner-scoped updates; false when the row is not owned by this user
    virtual bool setAttention(int64_t user_id, int64_t rec_id, bool attention) = 0;
    virtual bool remove(int64_t user_id, int64_t rec_id) = 0;
};

} // namespace ecshop::domain
