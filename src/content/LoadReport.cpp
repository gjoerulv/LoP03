#include "content/LoadReport.hpp"

#include <utility>

namespace cd::content {

void LoadReport::add(std::string source, std::string context, std::string message) {
    errors_.push_back({std::move(source), std::move(context), std::move(message)});
}

std::string LoadReport::summary() const {
    std::string out;
    for (const auto& e : errors_) {
        out += e.source;
        out += ": ";
        out += e.context;
        out += ": ";
        out += e.message;
        out += '\n';
    }
    return out;
}

}  // namespace cd::content
