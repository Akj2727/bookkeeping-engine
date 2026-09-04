#pragma once
// 8-decimal fixed point. Quantities and prices are int64 scaled by 1e8.
// Money is qty * price kept in __int128 at scale 1e16, so products are exact.
#include <cstdint>
#include <stdexcept>
#include <string>

namespace books {

using Fixed = int64_t;
using Amount = __int128;

constexpr Fixed kScale = 100'000'000;  // 1e8
constexpr int kDecimals = 8;

// Parses "123.45678901" or "-0.5" into fixed point without touching floats.
inline Fixed parse_fixed(const std::string& s) {
    size_t i = 0;
    bool neg = false;
    if (i < s.size() && (s[i] == '-' || s[i] == '+')) neg = s[i++] == '-';
    Fixed whole = 0, frac = 0;
    int digits = 0;
    bool seen_dot = false, any = false;
    for (; i < s.size(); ++i) {
        char c = s[i];
        if (c == '.' && !seen_dot) { seen_dot = true; continue; }
        if (c < '0' || c > '9') throw std::invalid_argument("bad number: " + s);
        any = true;
        if (!seen_dot) whole = whole * 10 + (c - '0');
        else if (digits < kDecimals) { frac = frac * 10 + (c - '0'); ++digits; }
        else throw std::invalid_argument("more than 8 decimals: " + s);
    }
    if (!any) throw std::invalid_argument("bad number: " + s);
    for (; digits < kDecimals; ++digits) frac *= 10;
    Fixed v = whole * kScale + frac;
    return neg ? -v : v;
}

inline std::string to_string(Amount v, int decimals) {
    bool neg = v < 0;
    unsigned __int128 u = neg ? static_cast<unsigned __int128>(-v) : static_cast<unsigned __int128>(v);
    std::string digits;
    do { digits.insert(digits.begin(), char('0' + int(u % 10))); u /= 10; } while (u);
    while (static_cast<int>(digits.size()) <= decimals) digits.insert(digits.begin(), '0');
    std::string out = digits.substr(0, digits.size() - decimals);
    if (decimals) out += "." + digits.substr(digits.size() - decimals);
    return neg ? "-" + out : out;
}

inline std::string fixed_str(Fixed v) { return to_string(v, 8); }
inline std::string amount_str(Amount v) { return to_string(v, 16); }

}  // namespace books
