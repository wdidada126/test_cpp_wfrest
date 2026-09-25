#pragma once

#include "ecshop/domain/Message.h"
#include "ecshop/infrastructure/db/Db.h"

#include <memory>

namespace ecshop::infra {

class SqlMessageRepository : public domain::MessageRepository
{
public:
    explicit SqlMessageRepository(std::shared_ptr<Db> db) : db_(std::move(db)) {}

    domain::MessagePage listPublic(int64_t offset, int64_t limit) override;
    int64_t add(int64_t user_id, const std::string &username, const std::string &email,
                int64_t type, const std::string &title, const std::string &content) override;
    domain::MessagePage listOfUser(int64_t user_id, int64_t offset, int64_t limit) override;
    bool remove(int64_t user_id, int64_t msg_id) override;

private:
    std::optional<domain::Message> firstReply(int64_t msg_id);
    std::shared_ptr<Db> db_;
};

} // namespace ecshop::infra
