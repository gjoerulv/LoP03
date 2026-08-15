#include "ui/TextInput.hpp"

#include <utility>

namespace cd::ui {

bool isAllowedNameChar(int codepoint) {
    if (codepoint >= 'a' && codepoint <= 'z') return true;
    if (codepoint >= 'A' && codepoint <= 'Z') return true;
    if (codepoint >= '0' && codepoint <= '9') return true;
    return codepoint == ' ' || codepoint == '-' || codepoint == '\'';
}

bool isAllowedPrintableChar(int codepoint) { return codepoint >= 32 && codepoint <= 126; }

bool isAllowedDigitChar(int codepoint) { return codepoint >= '0' && codepoint <= '9'; }

TextInput::TextInput(std::size_t maxLength, std::string initial, TextFilter filter)
    : maxLength_(maxLength), filter_(filter) {
    setValue(std::move(initial));
}

bool TextInput::allowed(int codepoint) const {
    switch (filter_) {
        case TextFilter::Printable: return isAllowedPrintableChar(codepoint);
        case TextFilter::Digits: return isAllowedDigitChar(codepoint);
        case TextFilter::Name: break;
    }
    return isAllowedNameChar(codepoint);
}

void TextInput::setValue(std::string value) {
    value_.clear();
    for (char c : value) {
        if (value_.size() >= maxLength_) {
            break;
        }
        if (allowed(static_cast<unsigned char>(c))) {
            value_.push_back(c);
        }
    }
}

void TextInput::appendCodepoint(int codepoint) {
    if (full() || !allowed(codepoint)) {
        return;
    }
    value_.push_back(static_cast<char>(codepoint));
}

void TextInput::backspace() {
    if (!value_.empty()) {
        value_.pop_back();
    }
}

void TextInput::clear() { value_.clear(); }

std::string TextInput::trimmed() const {
    const std::size_t first = value_.find_first_not_of(' ');
    if (first == std::string::npos) {
        return "";
    }
    const std::size_t last = value_.find_last_not_of(' ');
    return value_.substr(first, last - first + 1);
}

}  // namespace cd::ui
