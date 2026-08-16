#pragma once

#include <charconv>
#include <cstdint>
#include <string>

// Pure, raylib-free parsing for the Guild's manual seed entry (M88). The
// buffer is digits-only (ui::TextFilter::Digits), so the only questions left
// are emptiness and overflow.

namespace cd {

// Parses a digits-only string into a seed. Returns 0 for an empty or
// unparseable buffer — the caller keeps the previous seed in that case, so
// "type nothing and Confirm" is a no-op, never a surprise seed. A value past
// the uint64 range clamps to the maximum rather than failing: the typed intent
// was clearly "as big as it goes".
inline std::uint64_t parseSeedDigits(const std::string& digits) {
    if (digits.empty()) {
        return 0;
    }
    std::uint64_t value = 0;
    const char* first = digits.data();
    const char* last = first + digits.size();
    const std::from_chars_result r = std::from_chars(first, last, value);
    if (r.ec == std::errc::result_out_of_range) {
        return UINT64_MAX;
    }
    if (r.ec != std::errc() || r.ptr != last) {
        return 0;  // non-digit garbage cannot happen via the filter; be safe anyway
    }
    return value;
}

}  // namespace cd
