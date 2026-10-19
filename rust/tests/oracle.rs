use ledger_rs::*;
use std::collections::VecDeque;

/// Independent oracle: expands every trade into single units and matches unit by unit.
struct UnitOracle {
    units: Vec<VecDeque<(i64, i64)>>, // (sign, px)
    realized: Vec<i128>,
}

impl UnitOracle {
    fn new(n: usize) -> Self {
        UnitOracle { units: vec![VecDeque::new(); n], realized: vec![0; n] }
    }
    fn book(&mut self, asset: usize, units: i64, px: i64) {
        let sign = units.signum();
        for _ in 0..units.abs() {
            match self.units[asset].front() {
                Some(&(s, p)) if s != sign => {
                    self.units[asset].pop_front();
                    self.realized[asset] += s as i128 * SCALE as i128 * (px - p) as i128;
                }
                _ => self.units[asset].push_back((sign, px)),
            }
        }
    }
    fn position_units(&self, asset: usize) -> i64 {
        self.units[asset].iter().map(|u| u.0).sum()
    }
}

#[test]
fn matches_unit_oracle() {
    let n = 8;
    let mut rng = XorShift(0x9E3779B97F4A7C15);
    let mut l = Ledger::new(n);
    let mut o = UnitOracle::new(n);
    let mut px = vec![100 * SCALE; n];
    for _ in 0..200_000 {
        let a = rng.range(0, n as i64 - 1) as usize;
        px[a] = (px[a] + rng.range(-50_000_000, 50_000_000)).max(SCALE);
        let mut units = rng.range(-20, 20);
        if units == 0 {
            units = 1;
        }
        l.book(Trade { asset: a as u32, qty: units * SCALE, px: px[a] });
        o.book(a, units, px[a]);
    }
    for a in 0..n {
        assert_eq!(l.realized(a), o.realized[a], "realized pnl mismatch on asset {a}");
        assert_eq!(l.position(a), o.position_units(a) * SCALE, "position mismatch on asset {a}");
    }
    assert!(l.is_balanced());
}

#[test]
fn fractional_quantities_keep_identities() {
    let n = 16;
    let mut rng = XorShift(12345);
    let mut l = Ledger::new(n);
    let mut px = vec![50 * SCALE; n];
    for _ in 0..500_000 {
        let a = rng.range(0, n as i64 - 1) as usize;
        px[a] = (px[a] + rng.range(-3_000_000, 3_000_000)).max(SCALE / 100);
        let mut q = rng.range(-5 * SCALE, 5 * SCALE); // up to 8 decimals
        if q == 0 {
            q = 1;
        }
        l.book(Trade { asset: a as u32, qty: q, px: px[a] });
    }
    assert!(l.is_balanced(), "debits != credits");
    // Equity identity: cash + sum(position * mark) == realized + unrealized, exactly.
    let mut lhs = l.balance(Acct::Cash);
    let mut rhs: i128 = 0;
    for a in 0..n {
        let mark = px[a] + rng.range(-SCALE, SCALE);
        lhs += l.position(a) as i128 * mark as i128;
        rhs += l.realized(a) + l.unrealized(a, mark);
    }
    assert_eq!(lhs, rhs);
    // Realized PnL account mirrors per-asset realized PnL.
    let total: i128 = (0..n).map(|a| l.realized(a)).sum();
    assert_eq!(-l.balance(Acct::RealizedPnl), total);
}
