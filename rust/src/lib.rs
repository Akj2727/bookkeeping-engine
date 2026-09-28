//! Double-entry bookkeeping engine with FIFO lot matching.
//! Quantities and prices are 8-decimal fixed point (i64, scale 1e8).
//! Money amounts are qty * px in i128 (scale 1e16), so nothing is ever rounded.

use std::collections::VecDeque;

pub const SCALE: i64 = 100_000_000;

#[derive(Clone, Copy, Debug)]
pub struct Lot {
    pub qty: i64, // signed: + long, - short
    pub px: i64,
}

#[derive(Clone, Copy, Debug)]
pub struct Trade {
    pub asset: u32,
    pub qty: i64, // signed: + buy, - sell
    pub px: i64,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Acct {
    Cash = 0,
    Inventory = 1,
    RealizedPnl = 2,
}

pub struct Ledger {
    lots: Vec<VecDeque<Lot>>,
    realized: Vec<i128>,
    balances: [i128; 3], // debit-positive
    pub debits: i128,
    pub credits: i128,
    pub entries: u64,
}

impl Ledger {
    pub fn new(n_assets: usize) -> Self {
        Ledger {
            lots: (0..n_assets).map(|_| VecDeque::new()).collect(),
            realized: vec![0; n_assets],
            balances: [0; 3],
            debits: 0,
            credits: 0,
            entries: 0,
        }
    }

    #[inline]
    fn post(&mut self, acct: Acct, amt: i128) {
        if amt == 0 {
            return;
        }
        self.balances[acct as usize] += amt;
        if amt > 0 {
            self.debits += amt;
        } else {
            self.credits -= amt;
        }
        self.entries += 1;
    }

    /// Books one trade: matches against opposite-side lots FIFO, opens a lot
    /// with any remainder, and posts a balanced journal entry.
    #[inline]
    pub fn book(&mut self, t: Trade) {
        let a = t.asset as usize;
        let q = &mut self.lots[a];
        let mut rem = t.qty;
        let mut realized: i128 = 0;
        let mut basis_closed: i128 = 0;
        let mut opened: i128 = 0;

        while rem != 0 {
            if let Some(l) = q.front_mut() {
                if (l.qty > 0) != (rem > 0) {
                    let m = rem.abs().min(l.qty.abs());
                    let closed = l.qty.signum() * m; // signed qty of lot being closed
                    basis_closed += closed as i128 * l.px as i128;
                    realized += closed as i128 * (t.px - l.px) as i128;
                    l.qty -= closed;
                    rem += closed;
                    if l.qty == 0 {
                        q.pop_front();
                    }
                    continue;
                }
            }
            q.push_back(Lot { qty: rem, px: t.px });
            opened = rem as i128 * t.px as i128;
            rem = 0;
        }

        self.realized[a] += realized;
        // Cash moves by -qty*px; inventory carries signed cost basis; gains are credits.
        self.post(Acct::Cash, -(t.qty as i128 * t.px as i128));
        self.post(Acct::Inventory, opened - basis_closed);
        self.post(Acct::RealizedPnl, -realized);
    }

    pub fn balance(&self, acct: Acct) -> i128 {
        self.balances[acct as usize]
    }

    pub fn realized(&self, asset: usize) -> i128 {
        self.realized[asset]
    }

    pub fn position(&self, asset: usize) -> i64 {
        self.lots[asset].iter().map(|l| l.qty).sum()
    }

    /// Mark-to-market: unrealized PnL of every open lot at the given mark.
    pub fn unrealized(&self, asset: usize, mark: i64) -> i128 {
        self.lots[asset]
            .iter()
            .map(|l| l.qty as i128 * (mark - l.px) as i128)
            .sum()
    }

    pub fn is_balanced(&self) -> bool {
        self.debits == self.credits && self.balances.iter().sum::<i128>() == 0
    }
}

/// Small deterministic RNG so tests and benchmarks need no external crates.
pub struct XorShift(pub u64);
impl XorShift {
    #[inline]
    pub fn next(&mut self) -> u64 {
        let mut x = self.0;
        x ^= x << 13;
        x ^= x >> 7;
        x ^= x << 17;
        self.0 = x;
        x
    }
    #[inline]
    pub fn range(&mut self, lo: i64, hi: i64) -> i64 {
        lo + (self.next() % (hi - lo + 1) as u64) as i64
    }
}
