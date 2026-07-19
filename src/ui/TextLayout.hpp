#pragma once

#include <functional>
#include <string>
#include <vector>

// Pure text layout: font-aware measurement is injected, so wrapping and fit
// policy are unit-tested headlessly (raylib's MeasureText needs a window).
// Rendering adapters live in ui/UiDraw.

namespace cd::ui {

// Pixel width of a single-line string at a font size.
using TextMeasure = std::function<int(const std::string& text, int fontSize)>;

// Vertical distance between the tops of consecutive wrapped lines.
inline int lineHeight(int fontSize) { return fontSize + 3; }

// True if the whole string fits in maxWidth on one line.
bool fitsWidth(const std::string& text, int maxWidth, int fontSize, const TextMeasure& measure);

// Greedy word wrap into lines no wider than maxWidth. Explicit '\n' starts a
// new line (empty paragraphs are preserved as empty lines). A single token
// wider than maxWidth is broken at the widest fitting prefix (at least one
// character, never inside a UTF-8 sequence), so no line ever exceeds
// maxWidth as long as one character fits.
std::vector<std::string> wrapText(const std::string& text, int maxWidth, int fontSize,
                                  const TextMeasure& measure);

}  // namespace cd::ui
