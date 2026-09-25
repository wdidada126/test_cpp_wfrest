#pragma once

#include "ecshop/domain/Comment.h"
#include "ecshop/domain/Message.h"
#include "ecshop/infrastructure/db/Db.h"

#include <memory>
#include <optional>
#include <vector>

namespace ecshop::infra {

// moderation: comments and board messages (publish / hide / delete)
class SqlAdminModerationRepository
{
public:
    explicit SqlAdminModerationRepository(std::shared_ptr<Db> db) : db_(std::move(db)) {}

    // all comments incl. unpublished (status = 0)
    std::vector<domain::Comment> listComments(int64_t offset, int64_t limit);
    int64_t countComments();
    // false when the comment is missing
    bool setCommentStatus(int64_t comment_id, bool published);
    bool removeComment(int64_t comment_id);

    std::vector<domain::Message> listMessages(int64_t offset, int64_t limit);
    int64_t countMessages();
    bool setMessageStatus(int64_t msg_id, bool published);
    bool removeMessage(int64_t msg_id);

private:
    std::shared_ptr<Db> db_;
};

} // namespace ecshop::infra
