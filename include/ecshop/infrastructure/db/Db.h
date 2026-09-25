#pragma once

#include <cstdint>
#include <cstdlib>
#include <mutex>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace ecshop::infra {

// All values cross the boundary as text: DB drivers coerce on their side and
// Money/Time helpers normalize on ours. SQL must use '?' placeholders.
using Params = std::vector<std::string>;

class Row
{
public:
    void set(const std::string &column, std::string value)
    {
        cols_.emplace_back(column, std::move(value));
    }

    bool has(const std::string &column) const
    {
        for (const auto &c : cols_)
        {
            if (c.first == column)
                return true;
        }
        return false;
    }

    const std::string &get(const std::string &column) const
    {
        for (const auto &c : cols_)
        {
            if (c.first == column)
                return c.second;
        }
        static const std::string empty;
        return empty;
    }

    int64_t getInt(const std::string &column) const
    {
        const std::string &v = get(column);
        return v.empty() ? 0 : std::strtoll(v.c_str(), nullptr, 10);
    }

private:
    std::vector<std::pair<std::string, std::string>> cols_; // keeps select order
};

class DbError : public std::runtime_error
{
public:
    using std::runtime_error::runtime_error;
};

// Synchronous, mutex-guarded SQL connection shared by the route handlers.
// SQLite and MySQL implement the same '?'-parameterized interface; the few
// schema wording differences are exposed through the dialect methods below so
// repositories keep one SQL text per statement.
class Db
{
public:
    virtual ~Db() = default;

    virtual std::vector<Row> query(const std::string &sql, const Params &params) = 0;
    virtual int64_t execute(const std::string &sql, const Params &params) = 0; // rows affected
    virtual int64_t lastInsertId() = 0;

    void begin() { execute("BEGIN", {}); }
    void commit() { execute("COMMIT", {}); }
    void rollback() { execute("ROLLBACK", {}); }

    // Run fn inside a transaction; rolls back when fn throws.
    template <typename Fn>
    void transaction(Fn &&fn)
    {
        begin();
        try
        {
            fn();
            commit();
        }
        catch (...)
        {
            rollback();
            throw;
        }
    }

    // ---- dialect ----

    // logical table name -> physical one (sqlite keeps the legacy ecs_ prefix)
    virtual std::string table(const std::string &name) const = 0;

    // products.sku attribute column: sqlite "goods_attr", mysql "product_attr"
    virtual std::string productAttrCol() const = 0;

    // current unix timestamp as SQL expression
    virtual std::string unixNow() const = 0;

    // date/time column -> unix seconds expression (columns keep raw text in rows)
    virtual std::string toUnix(const std::string &expr) const = 0;

    // current time as a value comparable with a date/time column
    virtual std::string nowExpr() const = 0;

    // unix seconds -> text to bind into a date/time column (sqlite: int text,
    // mysql: "YYYY-MM-DD HH:MM:SS")
    virtual std::string datetimeFromUnix(int64_t unix_seconds) const = 0;

    // order_info creation time column: sqlite "created_at", mysql "add_time"
    virtual std::string orderTimeCol() const = 0;

    // random ordering expression: sqlite "RANDOM()", mysql "RAND()"
    virtual std::string randomOrderExpr() const = 0;

    // goods_gallery extra image columns (mysql schema only has img_url)
    virtual std::string galleryThumb(const std::string &alias) const = 0;
    virtual std::string galleryOriginal(const std::string &alias) const = 0;

    virtual const char *driverName() const = 0;

protected:
    std::mutex mu_;
};

} // namespace ecshop::infra
