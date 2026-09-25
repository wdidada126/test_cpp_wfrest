#pragma once

#include "ecshop/domain/Tag.h"
#include "ecshop/infrastructure/db/Db.h"

#include <memory>

namespace ecshop::infra {

class SqlTagRepository : public domain::TagRepository
{
public:
    explicit SqlTagRepository(std::shared_ptr<Db> db) : db_(std::move(db)) {}

    bool addTags(int64_t user_id, int64_t goods_id, const std::vector<std::string> &words,
                 std::vector<domain::TagCount> &stats) override;
    std::vector<domain::TagCount> listOfUser(int64_t user_id) override;
    void removeTag(int64_t user_id, const std::string &word) override;

private:
    std::vector<domain::TagCount> statsOfGoods(int64_t goods_id);
    std::shared_ptr<Db> db_;
};

} // namespace ecshop::infra
