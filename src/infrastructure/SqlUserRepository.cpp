#include "ecshop/infrastructure/SqlUserRepository.h"

namespace ecshop::infra {

using domain::User;

static User rowToUser(const Row &row)
{
    User user;
    user.user_id = row.getInt("user_id");
    user.username = row.get("user_name");
    user.email = row.get("email");
    user.created_at = row.get("created_at");
    return user;
}

std::optional<User> SqlUserRepository::findById(int64_t user_id)
{
    std::string sql = "SELECT user_id AS user_id, user_name AS user_name, email AS email," +
                      db_->toUnix("created_at") + " AS created_at FROM " + db_->table("users") +
                      " WHERE user_id = ?";
    std::vector<Row> rows = db_->query(sql, {std::to_string(user_id)});
    if (rows.empty())
        return std::nullopt;
    return rowToUser(rows.front());
}

std::optional<User> SqlUserRepository::findByUsername(const std::string &username)
{
    std::string sql = "SELECT user_id AS user_id, user_name AS user_name, email AS email," +
                      db_->toUnix("created_at") + " AS created_at FROM " + db_->table("users") +
                      " WHERE user_name = ?";
    std::vector<Row> rows = db_->query(sql, {username});
    if (rows.empty())
        return std::nullopt;
    return rowToUser(rows.front());
}

std::optional<User> SqlUserRepository::findByEmail(const std::string &email)
{
    std::string sql = "SELECT user_id AS user_id, user_name AS user_name, email AS email," +
                      db_->toUnix("created_at") + " AS created_at FROM " + db_->table("users") +
                      " WHERE email = ?";
    std::vector<Row> rows = db_->query(sql, {email});
    if (rows.empty())
        return std::nullopt;
    return rowToUser(rows.front());
}

bool SqlUserRepository::updateEmail(int64_t user_id, const std::string &email)
{
    std::optional<User> owner = findByEmail(email);
    if (owner && owner->user_id != user_id)
        return false;

    std::string sql = "UPDATE " + db_->table("users") + " SET email = ? WHERE user_id = ?";
    db_->execute(sql, {email, std::to_string(user_id)});
    return true;
}

std::optional<std::string> SqlUserRepository::passwordHashOf(int64_t user_id)
{
    std::string sql = "SELECT password_hash AS password_hash FROM " + db_->table("users") +
                      " WHERE user_id = ?";
    std::vector<Row> rows = db_->query(sql, {std::to_string(user_id)});
    if (rows.empty())
        return std::nullopt;
    return rows.front().get("password_hash");
}

int64_t SqlUserRepository::createUser(const std::string &username, const std::string &email,
                                      const std::string &password_hash)
{
    std::string sql = "INSERT INTO " + db_->table("users") +
                      " (user_name, email, password_hash) VALUES (?, ?, ?)";
    db_->execute(sql, {username, email, password_hash});
    return db_->lastInsertId();
}

bool SqlUserRepository::updatePasswordIfHashMatches(int64_t user_id,
                                                    const std::string &expected_hash,
                                                    const std::string &new_hash)
{
    std::string sql = "UPDATE " + db_->table("users") +
                      " SET password_hash = ? WHERE user_id = ? AND password_hash = ?";
    return db_->execute(sql, {new_hash, std::to_string(user_id), expected_hash}) > 0;
}

void SqlUserRepository::createSession(const std::string &token_hash, int64_t user_id)
{
    std::string sql = "INSERT INTO " + db_->table("sessions") +
                      " (token_hash, user_id) VALUES (?, ?)";
    db_->execute(sql, {token_hash, std::to_string(user_id)});
}

std::optional<int64_t> SqlUserRepository::userIdOfTokenHash(const std::string &token_hash)
{
    std::string sql = "SELECT user_id AS user_id FROM " + db_->table("sessions") +
                      " WHERE token_hash = ?";
    std::vector<Row> rows = db_->query(sql, {token_hash});
    if (rows.empty())
        return std::nullopt;
    return rows.front().getInt("user_id");
}

void SqlUserRepository::deleteSession(const std::string &token_hash)
{
    std::string sql = "DELETE FROM " + db_->table("sessions") + " WHERE token_hash = ?";
    db_->execute(sql, {token_hash});
}

void SqlUserRepository::deleteSessionsOfUser(int64_t user_id)
{
    std::string sql = "DELETE FROM " + db_->table("sessions") + " WHERE user_id = ?";
    db_->execute(sql, {std::to_string(user_id)});
}

} // namespace ecshop::infra
