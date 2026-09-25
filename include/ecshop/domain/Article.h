#pragma once

#include <cstdint>
#include <string>
#include <vector>

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

struct ArticlePage
{
    int64_t total = 0;
    std::vector<ArticleSummary> items;
};

struct ArticleCategory
{
    int64_t cat_id = 0;
    std::string name;
};

} // namespace ecshop::domain
