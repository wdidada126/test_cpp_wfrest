#pragma once

#include "ecshop/domain/ArticleRepository.h"
#include "ecshop/infrastructure/db/Db.h"

namespace ecshop::infra {

class SqlArticleRepository : public domain::ArticleRepository
{
public:
    explicit SqlArticleRepository(std::shared_ptr<Db> db) : db_(std::move(db)) {}

    std::optional<domain::Article> findOpenArticle(int64_t article_id) override;
    std::vector<domain::ArticleSummary> listLatestOpen(int64_t limit) override;

private:
    std::shared_ptr<Db> db_;
};

} // namespace ecshop::infra
