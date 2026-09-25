#include "ecshop/infrastructure/SqlCommentRepository.h"

namespace ecshop::infra {

using domain::Comment;

static Comment rowToComment(const Row &row)
{
    Comment comment;
    comment.comment_id = row.getInt("comment_id");
    comment.goods_id = row.getInt("id_value");
    comment.user_id = row.getInt("user_id");
    comment.user_name = row.get("user_name");
    comment.content = row.get("content");
    comment.add_time = row.get("add_time");
    return comment;
}

std::vector<Comment> SqlCommentRepository::listOfGoods(int64_t goods_id, int64_t offset,
                                                       int64_t limit)
{
    // limit/offset are validated integers formatted by us; user data stays bound.
    std::string sql =
        "SELECT comment_id AS comment_id, id_value AS id_value, user_id AS user_id,"
        " user_name AS user_name, content AS content, " +
        db_->toUnix("add_time") + " AS add_time FROM " + db_->table("comment") +
        " WHERE id_value = ? AND status = 1"
        " ORDER BY comment_id DESC LIMIT " + std::to_string(limit) +
        " OFFSET " + std::to_string(offset);

    std::vector<Comment> items;
    for (const Row &row : db_->query(sql, {std::to_string(goods_id)}))
        items.push_back(rowToComment(row));
    return items;
}

int64_t SqlCommentRepository::countOfGoods(int64_t goods_id)
{
    std::string sql = "SELECT COUNT(*) AS total FROM " + db_->table("comment") +
                      " WHERE id_value = ? AND status = 1";
    std::vector<Row> rows = db_->query(sql, {std::to_string(goods_id)});
    return rows.empty() ? 0 : rows.front().getInt("total");
}

bool SqlCommentRepository::add(int64_t goods_id, int64_t user_id, const std::string &user_name,
                               const std::string &content)
{
    // single INSERT with a saleable-goods EXISTS condition
    std::string sql =
        "INSERT INTO " + db_->table("comment") +
        " (comment_type, id_value, user_id, user_name, content, status)"
        " SELECT 0, ?, ?, ?, ?, 1 WHERE EXISTS (SELECT 1 FROM " + db_->table("goods") +
        " WHERE goods_id = ? AND is_on_sale = 1 AND is_delete = 0)";
    return db_->execute(sql, {std::to_string(goods_id), std::to_string(user_id), user_name,
                              content, std::to_string(goods_id)}) > 0;
}

std::vector<Comment> SqlCommentRepository::listOfUser(int64_t user_id, int64_t offset,
                                                      int64_t limit)
{
    std::string sql =
        "SELECT comment_id AS comment_id, id_value AS id_value, user_id AS user_id,"
        " user_name AS user_name, content AS content, " +
        db_->toUnix("add_time") + " AS add_time FROM " + db_->table("comment") +
        " WHERE user_id = ? AND status = 1"
        " ORDER BY comment_id DESC LIMIT " + std::to_string(limit) +
        " OFFSET " + std::to_string(offset);

    std::vector<Comment> items;
    for (const Row &row : db_->query(sql, {std::to_string(user_id)}))
        items.push_back(rowToComment(row));
    return items;
}

int64_t SqlCommentRepository::countOfUser(int64_t user_id)
{
    std::string sql = "SELECT COUNT(*) AS total FROM " + db_->table("comment") +
                      " WHERE user_id = ? AND status = 1";
    std::vector<Row> rows = db_->query(sql, {std::to_string(user_id)});
    return rows.empty() ? 0 : rows.front().getInt("total");
}

bool SqlCommentRepository::remove(int64_t user_id, int64_t comment_id)
{
    std::string sql = "DELETE FROM " + db_->table("comment") +
                      " WHERE comment_id = ? AND user_id = ?";
    return db_->execute(sql, {std::to_string(comment_id), std::to_string(user_id)}) > 0;
}

} // namespace ecshop::infra
