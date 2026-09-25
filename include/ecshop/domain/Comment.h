#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace ecshop::domain {

struct Comment
{
    int64_t comment_id = 0;
    int64_t goods_id = 0;
    int64_t user_id = 0;
    std::string user_name;
    std::string content;
    std::string add_time; // unix seconds as text
};

class CommentRepository
{
public:
    virtual ~CommentRepository() = default;

    // published comments of a goods, newest first
    virtual std::vector<Comment> listOfGoods(int64_t goods_id, int64_t offset, int64_t limit) = 0;
    virtual int64_t countOfGoods(int64_t goods_id) = 0;

    // dev config auto-publishes (status = 1); returns false when goods is
    // missing/off shelf
    virtual bool add(int64_t goods_id, int64_t user_id, const std::string &user_name,
                     const std::string &content) = 0;

    virtual std::vector<Comment> listOfUser(int64_t user_id, int64_t offset, int64_t limit) = 0;
    virtual int64_t countOfUser(int64_t user_id) = 0;

    virtual bool remove(int64_t user_id, int64_t comment_id) = 0;
};

} // namespace ecshop::domain
