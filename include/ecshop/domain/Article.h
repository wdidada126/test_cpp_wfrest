#pragma once

#include <cstdint>
#include <string>

namespace ecshop::domain {

struct ArticleSummary
{
    int64_t article_id = 0;
    std::string title;
    std::string author;
    std::string description;
};

struct Article
{
    int64_t article_id = 0;
    int64_t cat_id = 0;
    std::string title;
    std::string author;
    std::string description;
    std::string content;
    std::string keywords;
    bool is_open = false;
};

} // namespace ecshop::domain
