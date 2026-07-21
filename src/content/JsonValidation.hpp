#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>

#include "content/LoadReport.hpp"
#include "content/Stats.hpp"

// ObjectReader reads fields from one JSON object, validating type/range and
// appending readable, contextual errors to a LoadReport instead of throwing.
// Every accessor returns a usable fallback on error so parsing can continue and
// collect ALL problems in one pass.

namespace cd::content {

using Json = nlohmann::json;

class ObjectReader {
public:
    ObjectReader(const Json& object, std::string context, std::string source, LoadReport& report)
        : obj_(object), ctx_(std::move(context)), source_(std::move(source)), report_(report) {}

    bool isObject() const { return obj_.is_object(); }
    const std::string& context() const { return ctx_; }

    // Strings.
    std::string reqString(const char* key);
    std::string optString(const char* key, const std::string& fallback = "");

    // Integers (JSON must be an integer number, not a float).
    int reqInt(const char* key);
    int reqIntMin(const char* key, int minValue);
    int reqIntRange(const char* key, int minValue, int maxValue);
    int optInt(const char* key, int fallback);
    int optIntMin(const char* key, int minValue, int fallback);

    // Accepts integer or floating JSON numbers.
    float optFloat(const char* key, float fallback);

    // Optional boolean (absent -> fallback; non-bool -> error + fallback).
    bool optBool(const char* key, bool fallback);

    // String arrays (each element validated as a string).
    std::vector<std::string> optStringArray(const char* key);

    // Nested stat objects: { "hp", "attack", "magic", "defense", "speed" }.
    StatBlock reqStatBlock(const char* key);  // hp >= 1, others >= 0
    StatBlock optStatBlock(const char* key);  // all optional, default 0, any int
    StatGrowth optStatGrowth(const char* key);

    // Enums via a parse function. Required reports an error if absent.
    template <typename E>
    E reqEnum(const char* key, std::optional<E> (*parse)(std::string_view), const char* enumName) {
        const Json* p = find(key);
        if (p == nullptr) {
            err(key, std::string("missing required ") + enumName);
            return E{};
        }
        if (!p->is_string()) {
            err(key, std::string("expected ") + enumName + " string");
            return E{};
        }
        const auto s = p->get<std::string>();
        if (auto v = parse(s)) {
            return *v;
        }
        err(key, std::string("unknown ") + enumName + " '" + s + "'");
        return E{};
    }

    template <typename E>
    E optEnum(const char* key, std::optional<E> (*parse)(std::string_view), E fallback,
              const char* enumName) {
        const Json* p = find(key);
        if (p == nullptr) {
            return fallback;
        }
        if (!p->is_string()) {
            err(key, std::string("expected ") + enumName + " string");
            return fallback;
        }
        const auto s = p->get<std::string>();
        if (auto v = parse(s)) {
            return *v;
        }
        err(key, std::string("unknown ") + enumName + " '" + s + "'");
        return fallback;
    }

private:
    const Json* find(const char* key) const;
    void err(const char* key, const std::string& message);
    std::string fieldContext(const char* key) const;

    const Json& obj_;
    std::string ctx_;
    std::string source_;
    LoadReport& report_;
};

}  // namespace cd::content
