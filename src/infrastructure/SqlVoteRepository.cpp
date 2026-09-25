#include "ecshop/infrastructure/SqlVoteRepository.h"
#include "ecshop/shared/TimeUtil.h"

#include <algorithm>
#include <set>

namespace ecshop::infra {

using domain::Vote;
using domain::VoteOption;
using domain::VoteResponseStatus;

std::optional<Vote> SqlVoteRepository::findCurrent()
{
    std::string vote_t = db_->table("vote");
    std::string option_t = db_->table("vote_option");

    std::string sql =
        "SELECT vote_id AS vote_id, vote_name AS vote_name, start_time AS start_time,"
        " end_time AS end_time, can_multi AS can_multi, vote_count AS vote_count FROM " +
        vote_t + " WHERE start_time <= " + db_->unixNow() + " AND end_time >= " + db_->unixNow() +
        " ORDER BY vote_id DESC LIMIT 1";

    std::vector<Row> rows = db_->query(sql, {});
    if (rows.empty())
        return std::nullopt;

    Vote vote;
    const Row &row = rows.front();
    vote.vote_id = row.getInt("vote_id");
    vote.name = row.get("vote_name");
    vote.start_time = row.getInt("start_time");
    vote.end_time = row.getInt("end_time");
    vote.can_multi = row.getInt("can_multi") != 0;
    vote.total_count = row.getInt("vote_count");

    std::string options_sql =
        "SELECT option_id AS option_id, option_name AS option_name,"
        " option_count AS option_count FROM " + option_t +
        " WHERE vote_id = ? ORDER BY option_order, option_id";
    for (const Row &option_row : db_->query(options_sql, {std::to_string(vote.vote_id)}))
    {
        VoteOption option;
        option.option_id = option_row.getInt("option_id");
        option.name = option_row.get("option_name");
        option.count = option_row.getInt("option_count");
        vote.options.push_back(std::move(option));
    }
    return vote;
}

VoteResponseStatus SqlVoteRepository::recordResponse(int64_t vote_id, const std::string &ip_address,
                                                     const std::vector<int64_t> &option_ids)
{
    using domain::VoteResponseStatus;

    std::string vote_t = db_->table("vote");
    std::string option_t = db_->table("vote_option");
    std::string log_t = db_->table("vote_log");

    // single-choice votes accept exactly one option; multi up to 20, unique
    if (option_ids.empty() || option_ids.size() > 20 ||
        std::set(option_ids.begin(), option_ids.end()).size() != option_ids.size())
        return VoteResponseStatus::InvalidOptions;

    VoteResponseStatus status = VoteResponseStatus::VoteNotFound;

    db_->transaction([&] {
        std::vector<Row> votes = db_->query(
            "SELECT can_multi AS can_multi FROM " + vote_t +
                " WHERE vote_id = ? AND start_time <= " + db_->unixNow() +
                " AND end_time >= " + db_->unixNow(),
            {std::to_string(vote_id)});
        if (votes.empty())
            return;

        bool can_multi = votes.front().getInt("can_multi") != 0;
        if (!can_multi && option_ids.size() != 1)
        {
            status = VoteResponseStatus::InvalidOptions;
            return;
        }

        // all options must belong to this vote
        for (int64_t option_id : option_ids)
        {
            std::vector<Row> options = db_->query(
                "SELECT option_id AS option_id FROM " + option_t +
                    " WHERE option_id = ? AND vote_id = ?",
                {std::to_string(option_id), std::to_string(vote_id)});
            if (options.empty())
            {
                status = VoteResponseStatus::InvalidOptions;
                return;
            }
        }

        // unique (vote_id, ip_address) avoids double voting per client IP
        try
        {
            db_->execute("INSERT INTO " + log_t + " (vote_id, ip_address, vote_time) VALUES (?, ?, ?)",
                         {std::to_string(vote_id), ip_address,
                          std::to_string(shared::nowUnix())});
        }
        catch (const DbError &)
        {
            status = VoteResponseStatus::DuplicateIp;
            return;
        }

        for (int64_t option_id : option_ids)
        {
            db_->execute("UPDATE " + option_t +
                             " SET option_count = option_count + 1 WHERE option_id = ?",
                         {std::to_string(option_id)});
        }
        db_->execute("UPDATE " + vote_t + " SET vote_count = vote_count + " +
                         std::to_string(option_ids.size()) + " WHERE vote_id = ?",
                     {std::to_string(vote_id)});

        status = VoteResponseStatus::Recorded;
    });
    return status;
}

} // namespace ecshop::infra
