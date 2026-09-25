#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace ecshop::domain {

struct AccountRequest
{
    int64_t id = 0;
    std::string kind;   // deposit | withdrawal
    std::string amount; // decimal string
    std::string status; // pending_payment | pending_review | ...
    int64_t payment_id = 0;
    std::string note;
    std::string created_at; // ISO UTC, empty when unknown
    std::string paid_at;    // ISO UTC, empty when unpaid
};

struct Balance
{
    std::string available; // decimal string
    std::string frozen;    // decimal string
};

struct AccountTransaction
{
    int64_t log_id = 0;
    std::string available_delta; // signed decimal string
    std::string frozen_delta;    // signed decimal string
    std::string reason;
    std::string reference_type;
    int64_t reference_id = 0;
    std::string created_at;
};

struct AccountTransactionPage
{
    int64_t total = 0;
    Balance balance;
    std::vector<AccountTransaction> items;
};

struct AccountRequestPage
{
    int64_t total = 0;
    Balance balance;
    std::vector<AccountRequest> items;
};

} // namespace ecshop::domain
