#pragma once
// Open lots for one asset, matched FIFO. A position is either all long or all short,
// so a trade first closes opposite lots oldest-first and then opens a lot with any remainder.
#include <algorithm>
#include <deque>

#include "fixed.hpp"

namespace books {

struct Lot {
    Fixed qty;  // > 0 long, < 0 short
    Fixed px;
};

struct MatchResult {
    Amount realized = 0;      // PnL from closed quantity
    Amount basis_closed = 0;  // signed cost basis taken off the books
    Amount opened = 0;        // signed cost basis of the new lot
};

class LotBook {
public:
    MatchResult apply(Fixed qty, Fixed px) {
        MatchResult r;
        while (qty != 0) {
            if (!lots_.empty() && (lots_.front().qty > 0) != (qty > 0)) {
                Lot& l = lots_.front();
                Fixed m = std::min(qty < 0 ? -qty : qty, l.qty < 0 ? -l.qty : l.qty);
                Fixed closed = l.qty > 0 ? m : -m;  // signed size of the lot being closed
                r.basis_closed += Amount(closed) * l.px;
                r.realized += Amount(closed) * (px - l.px);
                l.qty -= closed;
                qty += closed;
                if (l.qty == 0) lots_.pop_front();
                continue;
            }
            lots_.push_back({qty, px});
            r.opened = Amount(qty) * px;
            qty = 0;
        }
        return r;
    }

    Fixed position() const {
        Fixed p = 0;
        for (const Lot& l : lots_) p += l.qty;
        return p;
    }

    // Mark-to-market of every open lot.
    Amount unrealized(Fixed mark) const {
        Amount u = 0;
        for (const Lot& l : lots_) u += Amount(l.qty) * (mark - l.px);
        return u;
    }

    const std::deque<Lot>& lots() const { return lots_; }

private:
    std::deque<Lot> lots_;
};

}  // namespace books
