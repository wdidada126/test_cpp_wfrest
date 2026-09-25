#include "ecshop/http/AuthUtil.h"
#include "ecshop/shared/Password.h"

namespace ecshop::api {

std::optional<int64_t> authenticate(const wfrest::HttpReq *req,
                                    const std::shared_ptr<domain::UserRepository> &users,
                                    ApiResponse &err)
{
    const std::string &header = req->header("Authorization");
    static const std::string kPrefix = "Bearer ";

    if (header.compare(0, kPrefix.size(), kPrefix) != 0)
    {
        err = ApiError::unauthenticated("missing Bearer token");
        return std::nullopt;
    }

    std::string token = header.substr(kPrefix.size());
    if (token.empty())
    {
        err = ApiError::unauthenticated("missing Bearer token");
        return std::nullopt;
    }

    std::optional<int64_t> user_id = users->userIdOfTokenHash(shared::sha256Hex(token));
    if (!user_id)
    {
        err = ApiError::unauthenticated("invalid or expired session");
        return std::nullopt;
    }
    return user_id;
}

std::string issueToken(const std::shared_ptr<domain::UserRepository> &users, int64_t user_id)
{
    std::string token = shared::randomTokenHex(32);
    users->createSession(shared::sha256Hex(token), user_id);
    return token;
}

} // namespace ecshop::api
