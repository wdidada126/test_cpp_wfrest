#include "ecshop/infrastructure/SqlArticleRepository.h"

namespace ecshop::infra {

std::optional<domain::Article> SqlArticleRepository::findOpenArticle(int64_t article_id)
{
    std::string sql =
        "SELECT article_id AS article_id, cat_id AS cat_id, title AS title,"
        " author AS author, article_desc AS article_desc, content AS content,"
        " keywords AS keywords, is_open AS is_open FROM " + db_->table("article") +
        " WHERE article_id = ? AND is_open = 1";

    std::vector<Row> rows = db_->query(sql, {std::to_string(article_id)});
    if (rows.empty())
        return std::nullopt;

    const Row &row = rows.front();
    domain::Article article;
    article.article_id = row.getInt("article_id");
    article.cat_id = row.getInt("cat_id");
    article.title = row.get("title");
    article.author = row.get("author");
    article.description = row.get("article_desc");
    article.content = row.get("content");
    article.keywords = row.get("keywords");
    article.is_open = true;
    return article;
}

std::vector<domain::ArticleSummary> SqlArticleRepository::listLatestOpen(int64_t limit)
{
    // limit is a validated integer formatted by us; no user data here.
    std::string sql =
        "SELECT article_id AS article_id, title AS title, author AS author,"
        " article_desc AS article_desc FROM " + db_->table("article") +
        " WHERE is_open = 1 ORDER BY article_id DESC LIMIT " + std::to_string(limit);

    std::vector<domain::ArticleSummary> items;
    for (const Row &row : db_->query(sql, {}))
    {
        domain::ArticleSummary item;
        item.article_id = row.getInt("article_id");
        item.title = row.get("title");
        item.author = row.get("author");
        item.description = row.get("article_desc");
        items.push_back(std::move(item));
    }
    return items;
}

} // namespace ecshop::infra
