#pragma once

#include "ecshop/domain/Catalog.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace ecshop::domain {

// RSS items for /feed.xml
struct FeedItem
{
    std::string title;
    std::string link; // path part, e.g. /api/v1/goods/12
    std::string description;
};

// active image ad row (media_type = 0)
struct AdImage
{
    int64_t ad_id = 0;
    int64_t position_id = 0;
    std::string name;
    std::string link;
    std::string code;
};

// Marketing activity reads shared by /feed.xml and /api/v1/promotions.
class FeedRepository
{
public:
    virtual ~FeedRepository() = default;

    // goods_activity rows of one act_type in the current time window
    // (act_type: 0=snatch 1=group_buy 2=auction 3=exchange 4=package)
    virtual std::vector<FeedItem> listActivityItems(int64_t act_type, int64_t limit) = 0;

    // favourable_activity rows in the current time window
    virtual std::vector<FeedItem> listFavourableItems(int64_t limit) = 0;

    // active image ads ordered by position and id (docs/03 /cycle-image.xml)
    virtual std::vector<AdImage> listImageAds() = 0;
};

} // namespace ecshop::domain
