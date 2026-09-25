#pragma once

#include "ecshop/domain/VoteRepository.h"
#include "ecshop/infrastructure/db/Db.h"

#include <memory>

namespace ecshop::infra {

class SqlVoteRepository : public domain::VoteRepository
{
public:
    explicit SqlVoteRepository(std::shared_ptr<Db> db) : db_(std::move(db)) {}

    std::optional<domain::Vote> findCurrent() override;
    domain::VoteResponseStatus recordResponse(int64_t vote_id, const std::string &ip_address,
                                              const std::vector<int64_t> &option_ids) override;

private:
    std::shared_ptr<Db> db_;
};

} // namespace ecshop::infra
