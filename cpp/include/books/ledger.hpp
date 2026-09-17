#pragma once
// Ledger: books trades into per-asset FIFO lot books and posts one balanced
// journal transaction per trade.
//   Cash         moves by -qty * px (buys pay, sells receive)
//   Inventory    carries the signed cost basis of open lots
//   RealizedPnl  gains are credits, losses are debits
#include <array>
#include <cstdint>
#include <vector>

#include "journal.hpp"
#include "lot_book.hpp"

namespace books {

struct Trade {
    uint32_t asset;
    Fixed qty;  // > 0 buy, < 0 sell
    Fixed px;
};

class Ledger {
public:
    explicit Ledger(size_t n_assets) : books_(n_assets), realized_(n_assets, 0) {}

    void book(const Trade& t) {
        MatchResult m = books_[t.asset].apply(t.qty, t.px);
        realized_[t.asset] += m.realized;
        journal_.post(std::array<Posting, 3>{{
            {Account::Cash, -(Amount(t.qty) * t.px)},
            {Account::Inventory, m.opened - m.basis_closed},
            {Account::RealizedPnl, -m.realized},
        }});
    }

    Fixed position(size_t asset) const { return books_[asset].position(); }
    Amount realized(size_t asset) const { return realized_[asset]; }
    Amount unrealized(size_t asset, Fixed mark) const { return books_[asset].unrealized(mark); }
    const Journal& journal() const { return journal_; }
    size_t assets() const { return books_.size(); }

private:
    std::vector<LotBook> books_;
    std::vector<Amount> realized_;
    Journal journal_;
};

}  // namespace books
