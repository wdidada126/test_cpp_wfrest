#include "ecshop/infrastructure/SqlAdminModerationRepository.h"

namespace ecshop::infra {

using domain::Comment;
using domain::Message;

std::vector<Comment> SqlAdminModerationRepository::listComments(int64_t offset, int64_t limit)
{
    std::string sql =
        "SELECT comment_id AS comment_id, id_value AS id_value, user_id AS user_id,"
        " user_name AS user_name, content AS content, status AS status," +
        db_->toUnix("add_time") + " AS add_time FROM " + db_->table("comment") +
        " ORDER BY comment_id DESC LIMIT " + std::to_string(limit) +
        " OFFSET " + std::to_string(offset);

    std::vector<Comment> items;
    for (const Row &row : db_->query(sql, {}))
    {
        Comment comment;
        comment.comment_id = row.getInt("comment_id");
        comment.goods_id = row.getInt("id_value");
        comment.user_id = row.getInt("user_id");
        comment.user_name = row.get("user_name");
        comment.content = row.get("content");
        comment.add_time = row.get("add_time");
        items.push_back(std::move(comment));
    }
    return items;
}

int64_t SqlAdminModerationRepository::countComments()
{
    std::vector<Row> rows = db_->query(
        "SELECT COUNT(*) AS total FROM " + db_->table("comment"), {});
    return rows.empty() ? 0 : rows.front().getInt("total");
}

bool SqlAdminModerationRepository::setCommentStatus(int64_t comment_id, bool published)
{
    return db_->execute("UPDATE " + db_->table("comment") + " SET status = ? WHERE comment_id = ?",
                        {published ? "1" : "0", std::to_string(comment_id)}) > 0;
}

bool SqlAdminModerationRepository::removeComment(int64_t comment_id)
{
    return db_->execute("DELETE FROM " + db_->table("comment") + " WHERE comment_id = ?",
                        {std::to_string(comment_id)}) > 0;
}

std::vector<Message> SqlAdminModerationRepository::listMessages(int64_t offset, int64_t limit)
{
    std::string sql =
        "SELECT msg_id AS msg_id, user_id AS user_id, user_name AS user_name,"
        " user_email AS user_email, msg_title AS msg_title, msg_content AS msg_content,"
        " msg_type AS msg_type, msg_status AS msg_status, order_id AS order_id," +
        db_->toUnix("msg_time") + " AS msg_time FROM " + db_->table("feedback") +
        " WHERE parent_id = 0 ORDER BY msg_id DESC LIMIT " + std::to_string(limit) +
        " OFFSET " + std::to_string(offset);

    std::vector<Message> items;
    for (const Row &row : db_->query(sql, {}))
    {
        Message message;
        message.msg_id = row.getInt("msg_id");
        message.user_id = row.getInt("user_id");
        message.username = row.get("user_name");
        message.email = row.get("user_email");
        message.title = row.get("msg_title");
        message.content = row.get("msg_content");
        message.type = row.getInt("msg_type");
        message.order_id = row.getInt("order_id");
        message.published = row.getInt("msg_status") != 0;
        message.created_at = row.get("msg_time");
        items.push_back(std::move(message));
    }
    return items;
}

int64_t SqlAdminModerationRepository::countMessages()
{
    std::vector<Row> rows = db_->query(
        "SELECT COUNT(*) AS total FROM " + db_->table("feedback") + " WHERE parent_id = 0", {});
    return rows.empty() ? 0 : rows.front().getInt("total");
}

bool SqlAdminModerationRepository::setMessageStatus(int64_t msg_id, bool published)
{
    return db_->execute(
               "UPDATE " + db_->table("feedback") +
                   " SET msg_status = ? WHERE msg_id = ? AND parent_id = 0",
               {published ? "1" : "0", std::to_string(msg_id)}) > 0;
}

bool SqlAdminModerationRepository::removeMessage(int64_t msg_id)
{
    bool ok = false;
    db_->transaction([&] {
        int64_t changed = db_->execute(
            "DELETE FROM " + db_->table("feedback") + " WHERE msg_id = ? AND parent_id = 0",
            {std::to_string(msg_id)});
        if (changed == 0)
            return;
        db_->execute("DELETE FROM " + db_->table("feedback") + " WHERE parent_id = ?",
                     {std::to_string(msg_id)});
        ok = true;
    });
    return ok;
}

} // namespace ecshop::infra
