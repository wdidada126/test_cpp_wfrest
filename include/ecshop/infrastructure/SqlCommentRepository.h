#pragma once

#include <string>
#include <vector>

#include "ecshop/domain/Comment.h"
#include "ecshop/infrastructure/db/Db.h"

#include <memory>

namespace ecshop::infra {

class SqlCommentRepository : public domain::CommentRepository
{
public:
    explicit SqlCommentRepository(std::shared_ptr<Db> db) : db_(std::move(db)) {}

    std::vector<domain::Comment> listOfGoods(int64_t goods_id, int64_t offset,
                                             int64_t limit) override;
    int64_t countOfGoods(int64_t goods_id) override;
    bool add(int64_t goods_id, int64_t user_id, const std::string &user_name,
             const std::string &content) override;
    std::vector<domain::Comment> listOfUser(int64_t user_id, int64_t offset,
                                            int64_t limit) override;
    int64_t countOfUser(int64_t user_id) override;
    bool remove(int64_t user_id, int64_t comment_id) override;

private:
    std::shared_ptr<Db> db_;
};

} // namespace ecshop::infra
