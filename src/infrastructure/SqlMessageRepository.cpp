#include "ecshop/infrastructure/SqlMessageRepository.h"

namespace ecshop::infra {

using domain::Message;
using domain::MessagePage;

static const char *kMsgCols =
    "msg_id AS msg_id, parent_id AS parent_id, user_id AS user_id, user_name AS user_name,"
    " user_email AS user_email, msg_title AS msg_title, msg_content AS msg_content,"
    " msg_type AS msg_type, order_id AS order_id";

static Message rowToMessage(const Row &row)
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
    message.created_at = row.get("msg_time");
    return message;
}

MessagePage SqlMessageRepository::listPublic(int64_t offset, int64_t limit)
{
    const std::string feedback_t = db_->table("feedback");

    MessagePage page;
    std::vector<Row> counts = db_->query(
        "SELECT COUNT(*) AS total FROM " + feedback_t +
            " WHERE msg_area = 1 AND msg_status = 1 AND parent_id = 0",
        {});
    if (!counts.empty())
        page.total = counts.front().getInt("total");

    // limit/offset are validated integers formatted by us; no user data here.
    std::string sql =
        "SELECT " + std::string(kMsgCols) + ", " + db_->toUnix("msg_time") + " AS msg_time FROM " +
        feedback_t + " WHERE msg_area = 1 AND msg_status = 1 AND parent_id = 0"
                     " ORDER BY msg_id DESC LIMIT " + std::to_string(limit) +
        " OFFSET " + std::to_string(offset);

    for (const Row &row : db_->query(sql, {}))
        page.items.push_back(rowToMessage(row));
    return page;
}

int64_t SqlMessageRepository::add(int64_t user_id, const std::string &username,
                                  const std::string &email, int64_t type,
                                  const std::string &title, const std::string &content)
{
    std::string sql = "INSERT INTO " + db_->table("feedback") +
                      " (parent_id, user_id, user_name, user_email, msg_title, msg_type,"
                      " msg_content, msg_area, msg_status) VALUES (0, ?, ?, ?, ?, ?, ?, 1, 1)";
    db_->execute(sql, {user_id > 0 ? std::to_string(user_id) : std::string("0"), username, email,
                       title, std::to_string(type), content});
    return db_->lastInsertId();
}

std::optional<Message> SqlMessageRepository::firstReply(int64_t msg_id)
{
    std::string sql =
        "SELECT " + std::string(kMsgCols) + ", " + db_->toUnix("msg_time") + " AS msg_time FROM " +
        db_->table("feedback") + " WHERE parent_id = ? ORDER BY msg_id ASC LIMIT 1";

    std::vector<Row> rows = db_->query(sql, {std::to_string(msg_id)});
    if (rows.empty())
        return std::nullopt;
    return rowToMessage(rows.front());
}

MessagePage SqlMessageRepository::listOfUser(int64_t user_id, int64_t offset, int64_t limit)
{
    const std::string feedback_t = db_->table("feedback");

    MessagePage page;
    std::vector<Row> counts = db_->query(
        "SELECT COUNT(*) AS total FROM " + feedback_t +
            " WHERE user_id = ? AND parent_id = 0",
        {std::to_string(user_id)});
    if (!counts.empty())
        page.total = counts.front().getInt("total");

    // limit/offset are validated integers formatted by us; user data stays bound.
    std::string sql =
        "SELECT " + std::string(kMsgCols) + ", " + db_->toUnix("msg_time") + " AS msg_time FROM " +
        feedback_t + " WHERE user_id = ? AND parent_id = 0"
                     " ORDER BY msg_id DESC LIMIT " + std::to_string(limit) +
        " OFFSET " + std::to_string(offset);

    for (const Row &row : db_->query(sql, {std::to_string(user_id)}))
    {
        Message message = rowToMessage(row);
        if (std::optional<Message> reply = firstReply(message.msg_id))
        {
            message.has_reply = true;
            message.reply_username = reply->username;
            message.reply_content = reply->content;
            message.reply_time = reply->created_at;
        }
        page.items.push_back(std::move(message));
    }
    return page;
}

bool SqlMessageRepository::remove(int64_t user_id, int64_t msg_id)
{
    const std::string feedback_t = db_->table("feedback");

    bool ok = false;
    db_->transaction([&] {
        std::vector<Row> rows = db_->query(
            "SELECT msg_id AS msg_id FROM " + feedback_t + " WHERE msg_id = ? AND user_id = ?",
            {std::to_string(msg_id), std::to_string(user_id)});
        if (rows.empty())
            return;

        // delete the message and its replies in one transaction
        db_->execute("DELETE FROM " + feedback_t + " WHERE parent_id = ?",
                     {std::to_string(msg_id)});
        db_->execute("DELETE FROM " + feedback_t + " WHERE msg_id = ? AND user_id = ?",
                     {std::to_string(msg_id), std::to_string(user_id)});
        ok = true;
    });
    return ok;
}

} // namespace ecshop::infra
