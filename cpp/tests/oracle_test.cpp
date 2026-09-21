// Checks the ledger three ways:
//  1. Against a unit-by-unit FIFO oracle on integer quantities (exact match).
//  2. Accounting identities on 8-decimal quantities (exact, no tolerance).
//  3. Fixed-point parsing and a few hand-worked trades.
#include <cstdio>
#include <deque>
#include <random>
#include <vector>

#include "books/ledger.hpp"

using namespace books;

static int failures = 0;
#define CHECK(cond)                                                       \
    do {                                                                  \
        if (!(cond)) {                                                    \
            std::printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); \
            ++failures;                                                   \
        }                                                                 \
    } while (0)

// Oracle: explodes each trade into single units and pairs them off one at a time.
struct UnitOracle {
    std::vector<std::deque<std::pair<int, Fixed>>> units;  // (sign, px)
    std::vector<Amount> realized;
    explicit UnitOracle(size_t n) : units(n), realized(n, 0) {}
    void book(size_t a, long n_units, Fixed px) {
        int sign = n_units > 0 ? 1 : -1;
        for (long i = 0; i < (n_units > 0 ? n_units : -n_units); ++i) {
            auto& q = units[a];
            if (!q.empty() && q.front().first != sign) {
                realized[a] += Amount(q.front().first) * kScale * (px - q.front().second);
                q.pop_front();
            } else {
                q.push_back({sign, px});
            }
        }
    }
    Fixed position(size_t a) const {
        Fixed p = 0;
        for (auto& u : units[a]) p += u.first * kScale;
        return p;
    }
};

static void matches_unit_oracle() {
    const size_t n = 8;
    std::mt19937_64 rng(7);
    Ledger led(n);
    UnitOracle o(n);
    std::vector<Fixed> px(n, 100 * kScale);
    for (int i = 0; i < 200000; ++i) {
        size_t a = rng() % n;
        px[a] = std::max<Fixed>(kScale, px[a] + Fixed(rng() % 100'000'001) - 50'000'000);
        long u = long(rng() % 41) - 20;
        if (u == 0) u = 1;
        led.book({uint32_t(a), u * kScale, px[a]});
        o.book(a, u, px[a]);
    }
    for (size_t a = 0; a < n; ++a) {
        CHECK(led.realized(a) == o.realized[a]);
        CHECK(led.position(a) == o.position(a));
    }
    CHECK(led.journal().reconciles());
    std::printf("unit oracle: 200000 trades, realized PnL and positions match exactly\n");
}

static void fractional_identities() {
    const size_t n = 16;
    std::mt19937_64 rng(12345);
    Ledger led(n);
    std::vector<Fixed> px(n, 50 * kScale);
    for (int i = 0; i < 500000; ++i) {
        size_t a = rng() % n;
        px[a] = std::max<Fixed>(kScale / 100, px[a] + Fixed(rng() % 6'000'001) - 3'000'000);
        Fixed q = Fixed(rng() % (10 * kScale + 1)) - 5 * kScale;  // up to 8 decimals
        if (q == 0) q = 1;
        led.book({uint32_t(a), q, px[a]});
    }
    CHECK(led.journal().reconciles());
    // Equity identity: cash + positions at mark == realized + unrealized.
    Amount lhs = led.journal().balance(Account::Cash), rhs = 0, total_realized = 0;
    for (size_t a = 0; a < n; ++a) {
        Fixed mark = px[a] + Fixed(rng() % (2 * kScale + 1)) - kScale;
        lhs += Amount(led.position(a)) * mark;
        rhs += led.realized(a) + led.unrealized(a, mark);
        total_realized += led.realized(a);
    }
    CHECK(lhs == rhs);
    CHECK(-led.journal().balance(Account::RealizedPnl) == total_realized);
    std::printf("identities: 500000 fractional trades, debits == credits, equity identity exact\n");
}

static void hand_worked() {
    CHECK(parse_fixed("1") == kScale);
    CHECK(parse_fixed("-0.5") == -kScale / 2);
    CHECK(parse_fixed("123.00000001") == 123 * kScale + 1);
    CHECK(fixed_str(parse_fixed("42.125")) == "42.12500000");

    Ledger led(1);
    led.book({0, parse_fixed("10"), parse_fixed("100")});  // buy 10 @ 100
    led.book({0, parse_fixed("4"), parse_fixed("110")});   // buy 4 @ 110
    led.book({0, parse_fixed("-12"), parse_fixed("105")}); // sell 12: 10 @ +5, 2 @ -5
    CHECK(amount_str(led.realized(0)) == "40.0000000000000000");
    CHECK(led.position(0) == parse_fixed("2"));
    led.book({0, parse_fixed("-5"), parse_fixed("90")});   // close 2 @ -20, open short 3 @ 90
    CHECK(amount_str(led.realized(0)) == "0.0000000000000000");
    CHECK(led.position(0) == parse_fixed("-3"));
    CHECK(amount_str(led.unrealized(0, parse_fixed("80"))) == "30.0000000000000000");
    CHECK(led.journal().reconciles());
    std::printf("hand-worked trades: ok\n");
}

int main() {
    hand_worked();
    matches_unit_oracle();
    fractional_identities();
    return failures ? 1 : 0;
}
