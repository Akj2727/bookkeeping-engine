# bookkeeping-engine

A double-entry trade ledger with FIFO lot matching, written in C++17 and ported to Rust.

- **Exact money.** Quantities and prices are 8-decimal fixed point (int64, scale 1e8). Money is qty × price held in a 128-bit integer at scale 1e16, so nothing is ever rounded and no floats are involved.
- **Double entry.** Every trade posts one balanced transaction across Cash, Inventory (signed cost basis of open lots) and Realized PnL. Debits always equal credits.
- **FIFO lots, long and short.** A trade closes opposite-side lots oldest-first, realizes PnL on the closed quantity, and opens a new lot with any remainder.
- **Mark-to-market.** Unrealized PnL of every open lot at any mark price.

```
cpp/   C++17 implementation, tests, benchmark (CMake)
rust/  Rust port with the same design, tests, benchmark (Cargo)
```

## Correctness

Both versions run the same checks:

- **Unit-by-unit FIFO oracle.** 200,000 random integer-quantity trades are also booked by an oracle that splits every trade into single units and pairs them off one at a time. Realized PnL and positions match exactly per asset.
- **Accounting identities on fractional trades.** 500,000 trades with 8-decimal quantities. Debits equal credits, the trial balance sums to zero, and `cash + Σ position × mark == realized + unrealized` holds exactly.
- **Hand-worked trades** (C++). Partial closes, a long flipping to a short, and fixed-point parsing.

## Performance

10M random trades across 1,000 assets on a 2-core Xeon @ 2.1 GHz. Throughput is the best of 5 runs. Latency is timed per trade, so it includes about 30 ns of timer overhead.

| Version | Throughput | p50 | p99 |
| --- | --- | --- | --- |
| C++17 (`-O2 -march=native`) | 23.2M trades/s | 73 to 75 ns | 187 to 196 ns |
| Rust (release, LTO) | 20.2M to 24.5M trades/s | 70 to 81 ns | 141 to 207 ns |

## Build

```
# C++
cmake -S cpp -B cpp/build -DCMAKE_BUILD_TYPE=Release && cmake --build cpp/build -j
cd cpp/build && ctest && ./bench

# Rust
cd rust && cargo test --release && cargo run --release
```
