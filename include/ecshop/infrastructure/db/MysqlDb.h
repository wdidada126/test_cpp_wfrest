#pragma once

#include <string>
#include <vector>

#include "ecshop/infrastructure/db/Db.h"

struct MYSQL;

namespace ecshop::infra {

class MysqlDb : public Db
{
public:
    struct Options
    {
        std::string host;
        int port = 3306;
        std::string user;
        std::string password;
        std::string database;
    };

    explicit MysqlDb(const Options &opts);
    ~MysqlDb() override;

    std::vector<Row> query(const std::string &sql, const Params &params) override;
    int64_t execute(const std::string &sql, const Params &params) override;
    int64_t lastInsertId() override;

    std::string table(const std::string &name) const override { return name; }
    std::string productAttrCol() const override { return "product_attr"; }
    std::string unixNow() const override { return "UNIX_TIMESTAMP()"; }
    std::string toUnix(const std::string &expr) const override { return "UNIX_TIMESTAMP(" + expr + ")"; }
    std::string nowExpr() const override { return "NOW()"; }
    std::string datetimeFromUnix(int64_t unix_seconds) const override;
    std::string orderTimeCol() const override { return "add_time"; }
    std::string randomOrderExpr() const override { return "RAND()"; }
    std::string galleryThumb(const std::string &alias) const override { return alias + ".img_url"; }
    std::string galleryOriginal(const std::string &alias) const override { return alias + ".img_url"; }
    const char *driverName() const override { return "mysql"; }

private:
    struct MYSQL *conn_ = nullptr;
    int64_t last_insert_id_ = 0;
};

} // namespace ecshop::infra
