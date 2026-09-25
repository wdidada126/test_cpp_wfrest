#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "ecshop/domain/FeedRepository.h"
#include "ecshop/infrastructure/db/Db.h"

namespace ecshop::infra {

class SqlFeedRepository : public domain::FeedRepository
{
public:
    explicit SqlFeedRepository(std::shared_ptr<Db> db) : db_(std::move(db)) {}

    std::vector<domain::FeedItem> listActivityItems(int64_t act_type, int64_t limit) override;
    std::vector<domain::FeedItem> listFavourableItems(int64_t limit) override;
    std::vector<domain::AdImage> listImageAds() override;
    std::optional<domain::Ad> findActive(int64_t ad_id) override;
    domain::AdClickResult recordClick(int64_t ad_id, const std::string &referer) override;

private:
    std::shared_ptr<Db> db_;
};

} // namespace ecshop::infra
