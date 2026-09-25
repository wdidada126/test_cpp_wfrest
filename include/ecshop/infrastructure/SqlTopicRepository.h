#pragma once

#include <optional>

#include "ecshop/domain/TopicRepository.h"
#include "ecshop/infrastructure/db/Db.h"

#include <memory>

namespace ecshop::infra {

class SqlTopicRepository : public domain::TopicRepository
{
public:
    explicit SqlTopicRepository(std::shared_ptr<Db> db) : db_(std::move(db)) {}

    std::optional<domain::Topic> find(int64_t topic_id) override;

private:
    std::shared_ptr<Db> db_;
};

} // namespace ecshop::infra
