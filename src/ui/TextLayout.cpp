#include "ui/TextLayout.hpp"

namespace cd::ui {

namespace {

bool isUtf8Continuation(unsigned char c) { return (c & 0xC0) == 0x80; }

// Largest prefix of `token` (in bytes, on a UTF-8 boundary, at least one
// codepoint) that fits in maxWidth. Returns the byte length of the prefix.
std::size_t fittingPrefix(const std::string& token, int maxWidth, int fontSize,
                          const TextMeasure& measure) {
    std::size_t best = 0;
    std::size_t i = 0;
    while (i < token.size()) {
        // Advance to the end of the current codepoint.
        std::size_t next = i + 1;
        while (next < token.size() && isUtf8Continuation(static_cast<unsigned char>(token[next]))) {
            ++next;
        }
        if (measure(token.substr(0, next), fontSize) <= maxWidth) {
            best = next;
            i = next;
        } else {
            break;
        }
    }
    if (best == 0) {
        // Not even one codepoint fits; emit one anyway so wrapping terminates.
        best = 1;
        while (best < token.size() && isUtf8Continuation(static_cast<unsigned char>(token[best]))) {
            ++best;
        }
    }
    return best;
}

void wrapParagraph(const std::string& paragraph, int maxWidth, int fontSize,
                   const TextMeasure& measure, std::vector<std::string>& out) {
    if (paragraph.empty()) {
        out.push_back("");
        return;
    }
    std::string line;
    std::size_t pos = 0;
    while (pos < paragraph.size()) {
        const std::size_t space = paragraph.find(' ', pos);
        std::string word = paragraph.substr(pos, space == std::string::npos ? std::string::npos
                                                                            : space - pos);
        pos = space == std::string::npos ? paragraph.size() : space + 1;
        if (word.empty()) {
            continue;  // collapse repeated spaces
        }
        // Break tokens that can never fit on their own line.
        while (measure(word, fontSize) > maxWidth) {
            if (!line.empty()) {
                out.push_back(line);
                line.clear();
            }
            const std::size_t cut = fittingPrefix(word, maxWidth, fontSize, measure);
            out.push_back(word.substr(0, cut));
            word.erase(0, cut);
        }
        if (word.empty()) {
            continue;
        }
        const std::string candidate = line.empty() ? word : line + " " + word;
        if (measure(candidate, fontSize) <= maxWidth) {
            line = candidate;
        } else {
            if (!line.empty()) {
                out.push_back(line);
            }
            line = word;
        }
    }
    if (!line.empty()) {
        out.push_back(line);
    }
}

}  // namespace

bool fitsWidth(const std::string& text, int maxWidth, int fontSize, const TextMeasure& measure) {
    return measure(text, fontSize) <= maxWidth;
}

std::vector<std::string> wrapText(const std::string& text, int maxWidth, int fontSize,
                                  const TextMeasure& measure) {
    std::vector<std::string> out;
    std::size_t start = 0;
    while (true) {
        const std::size_t nl = text.find('\n', start);
        const std::string paragraph =
            text.substr(start, nl == std::string::npos ? std::string::npos : nl - start);
        wrapParagraph(paragraph, maxWidth, fontSize, measure, out);
        if (nl == std::string::npos) {
            break;
        }
        start = nl + 1;
    }
    return out;
}

}  // namespace cd::ui
