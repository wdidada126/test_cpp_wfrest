#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace ecshop::domain {

struct Topic
{
    int64_t topic_id = 0;
    std::string title;
    std::string intro;
    int64_t start_time = 0;
    int64_t end_time = 0;
    std::string data; // stored as-is (JSON content decides rendering)
    std::string css;
    std::string topic_img;
    std::string title_pic;
    std::string base_style;
    std::string keywords;
    std::string description;
};

class TopicRepository
{
public:
    virtual ~TopicRepository() = default;

    virtual std::optional<Topic> find(int64_t topic_id) = 0;
};

} // namespace ecshop::domain
