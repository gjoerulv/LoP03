#pragma once

#include <cstdint>
#include <string>

// M87: the supported-glyph contract for the original bitmap font. Pure and
// raylib-free — the single authority three layers bind to:
//   - tools/asset_gen/generate_font.ps1 emits exactly these codepoints;
//   - tests/test_glyph_coverage.cpp asserts the shipped .fnt files cover them;
//   - the content lint asserts shipped/localized text uses only them.
// Scope is deliberately Western/Northern-European Latin (the planned
// translations): printable ASCII plus every Latin-1 Supplement letter and the
// Spanish inverted marks and French guillemets. No CJK, no combining marks,
// no general Unicode line breaking — wrapText remains a Latin-script wrapper.

namespace cd::ui::glyphs {

// True when the font ships a glyph for the codepoint.
constexpr bool isSupportedCodepoint(std::uint32_t cp) {
    if (cp >= 32 && cp <= 126) {
        return true;  // printable ASCII
    }
    if (cp >= 0xC0 && cp <= 0xFF) {
        return cp != 0xD7 && cp != 0xF7;  // Latin-1 letters (not the x / division signs)
    }
    return cp == 0xA1 || cp == 0xBF ||   // Spanish inverted ! ?
           cp == 0xAB || cp == 0xBB;     // French guillemets
}

// True when a codepoint may appear in authored content: every supported
// glyph plus '\n', which is semantic paragraph separation (never layout).
// Tabs and other control characters are rejected so they fail the content
// lint loudly instead of rendering as the '?' fallback glyph.
constexpr bool isAllowedInContent(std::uint32_t cp) {
    return cp == '\n' || isSupportedCodepoint(cp);
}

// Decodes the UTF-8 sequence starting at `pos` (advancing it past the
// sequence) and returns the codepoint, or kInvalidUtf8 for a malformed or
// overlong-truncated sequence (pos then advances by one byte so scanning
// always terminates).
inline constexpr std::uint32_t kInvalidUtf8 = 0xFFFFFFFFu;

inline std::uint32_t decodeUtf8(const std::string& text, std::size_t& pos) {
    const auto byteAt = [&text](std::size_t i) {
        return static_cast<std::uint32_t>(static_cast<unsigned char>(text[i]));
    };
    const std::uint32_t b0 = byteAt(pos);
    std::size_t need = 0;
    std::uint32_t cp = 0;
    if (b0 < 0x80) {
        ++pos;
        return b0;
    } else if ((b0 & 0xE0) == 0xC0) {
        need = 1;
        cp = b0 & 0x1F;
    } else if ((b0 & 0xF0) == 0xE0) {
        need = 2;
        cp = b0 & 0x0F;
    } else if ((b0 & 0xF8) == 0xF0) {
        need = 3;
        cp = b0 & 0x07;
    } else {
        ++pos;
        return kInvalidUtf8;  // stray continuation or invalid lead byte
    }
    if (pos + need >= text.size()) {
        ++pos;
        return kInvalidUtf8;  // truncated sequence
    }
    for (std::size_t i = 1; i <= need; ++i) {
        const std::uint32_t b = byteAt(pos + i);
        if ((b & 0xC0) != 0x80) {
            ++pos;
            return kInvalidUtf8;
        }
        cp = (cp << 6) | (b & 0x3F);
    }
    pos += need + 1;
    return cp;
}

// Scans UTF-8 text; returns 0 when every codepoint is allowed in content,
// otherwise the first offending codepoint (kInvalidUtf8 for malformed UTF-8).
inline std::uint32_t firstUnsupportedCodepoint(const std::string& text) {
    std::size_t pos = 0;
    while (pos < text.size()) {
        const std::uint32_t cp = decodeUtf8(text, pos);
        if (cp == kInvalidUtf8 || !isAllowedInContent(cp)) {
            return cp;
        }
    }
    return 0;
}

}  // namespace cd::ui::glyphs
