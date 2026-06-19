#pragma once

#include <cstddef>
#include <string>
#include <vector>

// Accumulates human-readable content errors. Loading NEVER throws to the caller
// and never crashes on bad data; problems land here instead.

namespace cd::content {

struct LoadError {
    std::string source;   // file path or "<memory>"
    std::string context;  // e.g. "skills[2] 'fireball'.power"
    std::string message;  // e.g. "expected integer >= 0"
};

class LoadReport {
public:
    void add(std::string source, std::string context, std::string message);

    bool ok() const { return errors_.empty(); }
    std::size_t errorCount() const { return errors_.size(); }
    const std::vector<LoadError>& errors() const { return errors_; }

    // One line per error: "source: context: message".
    std::string summary() const;

private:
    std::vector<LoadError> errors_;
};

}  // namespace cd::content
