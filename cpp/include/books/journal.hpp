#pragma once
// Double-entry journal. Every posting is debit-positive / credit-negative and
// each transaction must net to zero, so total debits always equal total credits.
#include <array>
#include <stdexcept>

#include "fixed.hpp"

namespace books {

enum class Account : int { Cash = 0, Inventory = 1, RealizedPnl = 2 };
constexpr int kAccounts = 3;

struct Posting {
    Account account;
    Amount amount;  // > 0 debit, < 0 credit
};

class Journal {
public:
    // Posts one balanced transaction. Zero lines are skipped.
    template <size_t N>
    void post(const std::array<Posting, N>& lines) {
        Amount net = 0;
        for (const auto& l : lines) net += l.amount;
        if (net != 0) throw std::logic_error("unbalanced transaction");
        for (const auto& l : lines) {
            if (l.amount == 0) continue;
            balances_[static_cast<int>(l.account)] += l.amount;
            if (l.amount > 0) debits_ += l.amount;
            else credits_ -= l.amount;
            ++lines_;
        }
        ++transactions_;
    }

    Amount balance(Account a) const { return balances_[static_cast<int>(a)]; }
    Amount debits() const { return debits_; }
    Amount credits() const { return credits_; }
    uint64_t lines() const { return lines_; }
    uint64_t transactions() const { return transactions_; }

    // Reconciliation: debits == credits and the trial balance sums to zero.
    bool reconciles() const {
        Amount sum = 0;
        for (Amount b : balances_) sum += b;
        return debits_ == credits_ && sum == 0;
    }

private:
    std::array<Amount, kAccounts> balances_{};
    Amount debits_ = 0, credits_ = 0;
    uint64_t lines_ = 0, transactions_ = 0;
};

}  // namespace books
