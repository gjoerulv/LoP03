#pragma once

#include <cstddef>
#include <string>

// Pure single-line text buffer for name entry. Accepts a restricted, safe set of
// characters and enforces a maximum length. No raylib; the owning state feeds it
// codepoints from the platform (GetCharPressed) and Backspace.

namespace cd::ui {

// Allowed in names: ASCII letters, digits, space, hyphen, apostrophe.
bool isAllowedNameChar(int codepoint);

class TextInput {
public:
    explicit TextInput(std::size_t maxLength = 12, std::string initial = "");

    void setValue(std::string value);  // truncates to maxLength, drops disallowed chars
    const std::string& value() const { return value_; }
    std::size_t maxLength() const { return maxLength_; }
    bool empty() const { return value_.empty(); }
    bool full() const { return value_.size() >= maxLength_; }

    void appendCodepoint(int codepoint);  // ignored if disallowed or full
    void backspace();
    void clear();

    // value() with leading/trailing spaces removed.
    std::string trimmed() const;

private:
    std::string value_;
    std::size_t maxLength_;
};

}  // namespace cd::ui
