// Throughput and per-trade latency on 10M random trades across 1,000 assets.
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <random>
#include <string>
#include <vector>

#include "books/ledger.hpp"

using namespace books;
using Clock = std::chrono::steady_clock;

int main(int argc, char** argv) {
    size_t n = argc > 1 ? std::stoull(argv[1]) : 10'000'000;
    size_t assets = 1000;
    std::mt19937_64 rng(42);
    std::vector<Fixed> px(assets, 100 * kScale);
    std::vector<Trade> trades(n);
    for (auto& t : trades) {
        size_t a = rng() % assets;
        px[a] = std::max<Fixed>(kScale, px[a] + Fixed(rng() % 10'000'001) - 5'000'000);
        Fixed q = Fixed(rng() % (20 * kScale + 1)) - 10 * kScale;
        if (q == 0) q = 1;
        t = {uint32_t(a), q, px[a]};
    }

    double best = 1e18;
    for (int r = 0; r < 5; ++r) {
        Ledger led(assets);
        auto t0 = Clock::now();
        for (const Trade& t : trades) led.book(t);
        std::chrono::duration<double> s = Clock::now() - t0;
        if (!led.journal().reconciles()) return 1;
        best = std::min(best, s.count());
    }
    std::printf("trades: %zu  assets: %zu\n", n, assets);
    std::printf("throughput: %.2fM trades/s (%.1f ns/trade avg)\n", n / best / 1e6, best * 1e9 / n);

    Ledger led(assets);
    std::vector<uint32_t> ns;
    ns.reserve(n);
    for (const Trade& t : trades) {
        auto t0 = Clock::now();
        led.book(t);
        ns.push_back(uint32_t(std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - t0).count()));
    }
    std::sort(ns.begin(), ns.end());
    auto p = [&](double q) { return ns[size_t((ns.size() - 1) * q)]; };
    std::printf("latency p50 %u ns  p99 %u ns  p99.9 %u ns\n", p(0.5), p(0.99), p(0.999));
    std::printf("journal: %llu lines, reconciles: %s\n", (unsigned long long)led.journal().lines(),
                led.journal().reconciles() ? "yes" : "no");
}
