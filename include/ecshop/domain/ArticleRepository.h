#pragma once

#include "ecshop/domain/Article.h"

#include <memory>
#include <optional>
#include <vector>

namespace ecshop::domain {

// Public article reads (is_open = 1).
class ArticleRepository
{
public:
    virtual ~ArticleRepository() = default;

    virtual std::optional<Article> findOpenArticle(int64_t article_id) = 0;

    virtual std::vector<ArticleSummary> listLatestOpen(int64_t limit) = 0;

    // visible article category means existing and is_show = 1
    virtual bool categoryVisible(int64_t cat_id) = 0;

    // open articles of one article category, paged
    virtual ArticlePage listCategoryArticles(int64_t cat_id, int64_t offset, int64_t limit) = 0;
};

} // namespace ecshop::domain
