#pragma once

#include <cctype>
#include <string>
#include <vector>

// M120: the title screen's Credits & Licenses page — the pure side. The page
// shows the text the package ships (packaging/LICENSES.txt, embedded as
// core/Licenses.hpp); this header turns that hard-wrapped plain-text file into
// prose the scrollable viewport can wrap to ITS width: paragraphs are rejoined,
// while headings, copyright lines and numbered clauses keep their own lines.
// Raylib-free and unit-tested; the state only lays the result out.

namespace cd::credits {

// The working credit (owner direction, 2026-09-18). LICENSES.txt carries the
// same sentence — a test binds the two so they cannot drift apart.
inline constexpr const char* kCreditLine = "A game by SmettPlay";

namespace detail {

inline std::string trimmed(const std::string& s) {
    std::size_t a = 0;
    std::size_t b = s.size();
    while (a < b && (s[a] == ' ' || s[a] == '\t' || s[a] == '\r')) {
        ++a;
    }
    while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t' || s[b - 1] == '\r')) {
        --b;
    }
    return s.substr(a, b - a);
}

inline int indentOf(const std::string& s) {
    int n = 0;
    while (n < static_cast<int>(s.size()) && s[static_cast<std::size_t>(n)] == ' ') {
        ++n;
    }
    return n;
}

// "====" / "----" rules under a plain-text heading: layout, not content.
inline bool isUnderline(const std::string& t) {
    if (t.size() < 3) {
        return false;
    }
    for (char c : t) {
        if (c != '=' && c != '-') {
            return false;
        }
    }
    return true;
}

// "1. ..." — a numbered license clause.
inline bool isNumbered(const std::string& t) {
    std::size_t i = 0;
    while (i < t.size() && std::isdigit(static_cast<unsigned char>(t[i])) != 0) {
        ++i;
    }
    return i > 0 && i < t.size() && t[i] == '.';
}

inline bool startsWith(const std::string& t, const char* prefix) {
    return t.rfind(prefix, 0) == 0;
}

}  // namespace detail

// Reflows the shipped plain text for the page. Dropped: the file's own title
// and its underline (the page has a header band) and the credit sentence (the
// page draws it on its own line). Kept on their own lines: every unindented
// line that opens a block, each "Copyright ..." line, each numbered clause.
// Everything else joins the line above with one space; a blank line is one
// paragraph break however many were authored.
inline std::string bodyFromLicenses(const std::string& raw) {
    std::vector<std::string> lines;
    std::string current;
    for (char c : raw) {
        if (c == '\n') {
            lines.push_back(current);
            current.clear();
        } else {
            current += c;
        }
    }
    if (!current.empty()) {
        lines.push_back(current);
    }

    std::string out;
    bool lineOpen = false;      // a display line is being built
    bool forceBreak = false;    // the next text line must start a new display line
    bool pendingBlank = false;  // a paragraph break is owed before the next text
    int prevIndent = 0;
    bool first = true;
    for (const std::string& rawLine : lines) {
        const std::string t = detail::trimmed(rawLine);
        if (t.empty()) {
            pendingBlank = lineOpen || !out.empty();
            lineOpen = false;
            continue;
        }
        if (detail::isUnderline(t)) {
            continue;
        }
        if (first) {
            first = false;  // the file's own title line
            continue;
        }
        if (detail::startsWith(t, kCreditLine)) {
            continue;  // drawn by the page on its own line
        }
        const int indent = detail::indentOf(rawLine);
        const bool ownLine = detail::startsWith(t, "Copyright");
        const bool opens = !lineOpen || forceBreak || ownLine || detail::isNumbered(t) ||
                           (indent == 0) != (prevIndent == 0);
        if (opens) {
            if (!out.empty()) {
                out += pendingBlank ? "\n\n" : "\n";
            }
            out += t;
        } else {
            out += ' ';
            out += t;
        }
        pendingBlank = false;
        lineOpen = true;
        forceBreak = ownLine;
        prevIndent = indent;
    }
    return out;
}

}  // namespace cd::credits
