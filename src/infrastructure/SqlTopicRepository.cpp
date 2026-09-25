#include "ecshop/infrastructure/SqlTopicRepository.h"

namespace ecshop::infra {

std::optional<domain::Topic> SqlTopicRepository::find(int64_t topic_id)
{
    std::string sql =
        "SELECT topic_id AS topic_id, title AS title, intro AS intro,"
        " start_time AS start_time, end_time AS end_time, data AS data, css AS css,"
        " topic_img AS topic_img, title_pic AS title_pic, base_style AS base_style,"
        " keywords AS keywords, description AS description FROM " + db_->table("topic") +
        " WHERE topic_id = ?";

    std::vector<Row> rows = db_->query(sql, {std::to_string(topic_id)});
    if (rows.empty())
        return std::nullopt;

    const Row &row = rows.front();
    domain::Topic topic;
    topic.topic_id = row.getInt("topic_id");
    topic.title = row.get("title");
    topic.intro = row.get("intro");
    topic.start_time = row.getInt("start_time");
    topic.end_time = row.getInt("end_time");
    topic.data = row.get("data");
    topic.css = row.get("css");
    topic.topic_img = row.get("topic_img");
    topic.title_pic = row.get("title_pic");
    topic.base_style = row.get("base_style");
    topic.keywords = row.get("keywords");
    topic.description = row.get("description");
    return topic;
}

} // namespace ecshop::infra
