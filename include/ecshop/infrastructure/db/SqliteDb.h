#pragma once

#include "ecshop/infrastructure/db/Db.h"

#include <memory>

struct sqlite3;

namespace ecshop::infra {

class SqliteDb : public Db
{
public:
    explicit SqliteDb(const std::string &path);
    ~SqliteDb() override;

    std::vector<Row> query(const std::string &sql, const Params &params) override;
    int64_t execute(const std::string &sql, const Params &params) override;
    int64_t lastInsertId() override;

    std::string table(const std::string &name) const override { return "ecs_" + name; }
    std::string productAttrCol() const override { return "goods_attr"; }
    std::string unixNow() const override { return "unixepoch()"; }
    std::string toUnix(const std::string &expr) const override { return expr; }
    std::string nowExpr() const override { return "unixepoch()"; }
    std::string datetimeFromUnix(int64_t unix_seconds) const override
    {
        return std::to_string(unix_seconds);
    }
    std::string orderTimeCol() const override { return "created_at"; }
    std::string galleryThumb(const std::string &alias) const override { return alias + ".thumb_url"; }
    std::string galleryOriginal(const std::string &alias) const override { return alias + ".img_original"; }
    const char *driverName() const override { return "sqlite"; }

    // Create the schema when the file is new/empty (dev convenience).
    void ensureSchema();

private:
    struct sqlite3 *db_ = nullptr;
};

} // namespace ecshop::infra
