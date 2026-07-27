#include "editor/CanonicalJson.hpp"

namespace cd::editor {

namespace {

std::string spaces(int n) { return std::string(static_cast<std::size_t>(n), ' '); }

// A scalar exactly as nlohmann would emit it (string escaping, number
// formatting — 12.0 stays "12.0", so class growth floats round-trip).
std::string scalar(const OrderedJson& v) { return v.dump(); }

// Compact one-line rendering. Objects pad inside the braces
// (`{ "k": v }`); arrays pad only when their elements are objects
// (`[ { ... } ]` vs `["a", "b"]`) — the two shapes the shipped files use.
std::string compact(const OrderedJson& v) {
    if (v.is_object()) {
        if (v.empty()) {
            return "{}";
        }
        std::string out = "{ ";
        bool first = true;
        for (auto it = v.begin(); it != v.end(); ++it) {
            if (!first) {
                out += ", ";
            }
            first = false;
            out += OrderedJson(it.key()).dump();
            out += ": ";
            out += compact(it.value());
        }
        out += " }";
        return out;
    }
    if (v.is_array()) {
        if (v.empty()) {
            return "[]";
        }
        const bool objects = v.front().is_object();
        std::string out = objects ? "[ " : "[";
        bool first = true;
        for (const OrderedJson& el : v) {
            if (!first) {
                out += ", ";
            }
            first = false;
            out += compact(el);
        }
        out += objects ? " ]" : "]";
        return out;
    }
    return scalar(v);
}

// True when every element of a non-empty array is an object — the one case a
// value inside a block entity goes multi-line (learnsets, item statuses).
bool isObjectArray(const OrderedJson& v) {
    if (!v.is_array() || v.empty()) {
        return false;
    }
    for (const OrderedJson& el : v) {
        if (!el.is_object()) {
            return false;
        }
    }
    return true;
}

// Multi-line object: "{" on the caller's line, one key per line at indent+2,
// closing "}" at indent. Nested values stay inline except arrays of objects,
// which get one compact object per line (the classes.json learnset shape).
std::string blockObject(const OrderedJson& obj, int indent) {
    if (obj.empty()) {
        return "{}";
    }
    std::string out = "{\n";
    bool first = true;
    for (auto it = obj.begin(); it != obj.end(); ++it) {
        if (!first) {
            out += ",\n";
        }
        first = false;
        out += spaces(indent + 2);
        out += OrderedJson(it.key()).dump();
        out += ": ";
        const OrderedJson& v = it.value();
        if (isObjectArray(v)) {
            out += "[\n";
            bool firstEl = true;
            for (const OrderedJson& el : v) {
                if (!firstEl) {
                    out += ",\n";
                }
                firstEl = false;
                out += spaces(indent + 4);
                out += compact(el);
            }
            out += "\n" + spaces(indent + 2) + "]";
        } else {
            out += compact(v);
        }
    }
    out += "\n" + spaces(indent) + "}";
    return out;
}

std::string entityArray(const OrderedJson& arr, FileStyle style) {
    if (arr.empty()) {
        return "[]";
    }
    std::string out = "[\n";
    bool first = true;
    for (const OrderedJson& el : arr) {
        if (!first) {
            out += ",\n";
        }
        first = false;
        out += spaces(4);
        if (style == FileStyle::BlockEntities && el.is_object()) {
            out += blockObject(el, 4);
        } else {
            out += compact(el);
        }
    }
    out += "\n  ]";
    return out;
}

}  // namespace

std::string canonicalize(const OrderedJson& root, FileStyle style) {
    if (!root.is_object()) {
        // Defensive: a malformed root still round-trips losslessly.
        return root.dump(2) + "\n";
    }
    std::string out = "{\n";
    bool first = true;
    for (auto it = root.begin(); it != root.end(); ++it) {
        if (!first) {
            out += ",\n";
        }
        first = false;
        out += "  ";
        out += OrderedJson(it.key()).dump();
        out += ": ";
        const OrderedJson& v = it.value();
        if (v.is_array()) {
            out += entityArray(v, style);
        } else if (v.is_object()) {
            // composition.json's team/boss/statScale sections.
            out += blockObject(v, 2);
        } else {
            out += scalar(v);
        }
    }
    out += "\n}\n";
    return out;
}

FileStyle styleForFile(const std::string& filename) {
    if (filename == "skills.json" || filename == "enemies.json" || filename == "items.json" ||
        filename == "passives.json" || filename == "milestones.json") {
        return FileStyle::InlineEntities;
    }
    return FileStyle::BlockEntities;
}

}  // namespace cd::editor
