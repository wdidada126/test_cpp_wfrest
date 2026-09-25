#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace ecshop::domain {

struct TagCount
{
    std::string word;
    int64_t count = 0;
};

class TagRepository
{
public:
    virtual ~TagRepository() = default;

    // insert tags for a visible goods; duplicates (same user+goods+word) are
    // ignored. Returns the goods tag statistics afterwards. ok=false when the
    // goods is not on sale.
    virtual bool addTags(int64_t user_id, int64_t goods_id,
                         const std::vector<std::string> &words,
                         std::vector<TagCount> &stats) = 0;

    // tag words of a user, aggregated with usage counts
    virtual std::vector<TagCount> listOfUser(int64_t user_id) = 0;

    // public tag cloud: only tags of on-sale, non-deleted goods
    virtual std::vector<TagCount> listPublic() = 0;

    // deletes only the current user's rows; idempotent
    virtual void removeTag(int64_t user_id, const std::string &word) = 0;
};

} // namespace ecshop::domain
