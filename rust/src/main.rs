use ledger_rs::*;
use std::hint::black_box;
use std::time::Instant;

fn gen(n_trades: usize, n_assets: usize, seed: u64) -> Vec<Trade> {
    let mut rng = XorShift(seed);
    let mut px = vec![100 * SCALE; n_assets];
    (0..n_trades)
        .map(|_| {
            let a = rng.range(0, n_assets as i64 - 1) as usize;
            px[a] = (px[a] + rng.range(-5_000_000, 5_000_000)).max(SCALE);
            let mut q = rng.range(-10 * SCALE, 10 * SCALE);
            if q == 0 {
                q = 1;
            }
            Trade { asset: a as u32, qty: q, px: px[a] }
        })
        .collect()
}

fn main() {
    let n_trades = 10_000_000;
    let n_assets = 1_000;
    let trades = gen(n_trades, n_assets, 42);

    // Throughput: best of 5 runs.
    let mut best = f64::MAX;
    for _ in 0..5 {
        let mut l = Ledger::new(n_assets);
        let t0 = Instant::now();
        for &t in &trades {
            l.book(t);
        }
        let s = t0.elapsed().as_secs_f64();
        assert!(l.is_balanced());
        black_box(&l);
        best = best.min(s);
    }
    println!("trades: {n_trades}, assets: {n_assets}");
    println!("throughput: {:.2}M trades/s ({:.1} ns/trade avg)", n_trades as f64 / best / 1e6, best * 1e9 / n_trades as f64);

    // Per-trade latency, with timer overhead measured separately.
    let mut overhead: Vec<u64> = (0..1_000_000)
        .map(|_| {
            let t0 = Instant::now();
            black_box(());
            t0.elapsed().as_nanos() as u64
        })
        .collect();
    overhead.sort_unstable();
    let oh = overhead[overhead.len() / 2];

    let mut l = Ledger::new(n_assets);
    let mut lat: Vec<u64> = Vec::with_capacity(n_trades);
    for &t in &trades {
        let t0 = Instant::now();
        l.book(black_box(t));
        lat.push(t0.elapsed().as_nanos() as u64);
    }
    lat.sort_unstable();
    let p = |q: f64| lat[((lat.len() as f64 - 1.0) * q) as usize];
    println!("timer overhead p50: {oh} ns");
    println!("latency raw   p50: {} ns, p99: {} ns, p99.9: {} ns", p(0.5), p(0.99), p(0.999));
    println!(
        "latency net   p50: {} ns, p99: {} ns",
        p(0.5).saturating_sub(oh),
        p(0.99).saturating_sub(oh)
    );
    println!("journal lines posted: {}", l.entries);
}
