#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace ecshop::domain {

struct VoteOption
{
    int64_t option_id = 0;
    std::string name;
    int64_t count = 0;
};

struct Vote
{
    int64_t vote_id = 0;
    std::string name;
    int64_t start_time = 0;
    int64_t end_time = 0;
    bool can_multi = false;
    int64_t total_count = 0;
    std::vector<VoteOption> options;
};

enum class VoteResponseStatus
{
    Recorded,
    DuplicateIp,
    VoteNotFound,
    VoteClosed,
    InvalidOptions,
};

class VoteRepository
{
public:
    virtual ~VoteRepository() = default;

    // current in-window vote with option counts
    virtual std::optional<Vote> findCurrent() = 0;

    // records one client vote; validates window, options and single/multi rules
    virtual VoteResponseStatus recordResponse(int64_t vote_id, const std::string &ip_address,
                                              const std::vector<int64_t> &option_ids) = 0;
};

} // namespace ecshop::domain
