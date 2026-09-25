#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace ecshop::domain {

struct Message
{
    int64_t msg_id = 0;
    int64_t user_id = 0;
    std::string username;
    std::string email;
    std::string title;
    std::string content;
    int64_t type = 0;
    int64_t order_id = 0;
    std::string created_at; // unix seconds as text
    bool published = true; // msg_status (admin moderation view uses this)

    // /me/messages carries the first admin/customer-service reply (if any)
    bool has_reply = false;
    std::string reply_username;
    std::string reply_content;
    std::string reply_time; // unix seconds as text
};

struct MessagePage
{
    int64_t total = 0;
    std::vector<Message> items;
};

class MessageRepository
{
public:
    virtual ~MessageRepository() = default;

    // public board: msg_area = 1 and msg_status = 1
    virtual MessagePage listPublic(int64_t offset, int64_t limit) = 0;

    // create; parent_id 0 = top-level board message
    virtual int64_t add(int64_t user_id, const std::string &username, const std::string &email,
                        int64_t type, const std::string &title, const std::string &content) = 0;

    // own top-level messages with their first reply
    virtual MessagePage listOfUser(int64_t user_id, int64_t offset, int64_t limit) = 0;

    // deletes the message and its replies; false when not owned
    virtual bool remove(int64_t user_id, int64_t msg_id) = 0;
};

} // namespace ecshop::domain
