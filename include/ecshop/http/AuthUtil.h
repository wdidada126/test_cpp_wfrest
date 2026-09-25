#pragma once

#include "ecshop/domain/UserRepository.h"
#include "ecshop/http/ApiError.h"

#include "wfrest/HttpMsg.h"

#include <memory>
#include <optional>

namespace ecshop::api {

// Resolves the Bearer token of a request to a user id.
// Filled-in err is the 401 response to send when authentication fails.
std::optional<int64_t> authenticate(const wfrest::HttpReq *req,
                                    const std::shared_ptr<domain::UserRepository> &users,
                                    ApiResponse &err);

// Issues a fresh access token and stores its hash; returns the plaintext token
// (the only time it is ever returned).
std::string issueToken(const std::shared_ptr<domain::UserRepository> &users, int64_t user_id);

} // namespace ecshop::api
