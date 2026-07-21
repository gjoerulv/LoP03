#include "content/JsonValidation.hpp"

namespace cd::content {

const Json* ObjectReader::find(const char* key) const {
    // json::find returns end() for non-objects, so this is always safe.
    auto it = obj_.find(key);
    if (it == obj_.end()) {
        return nullptr;
    }
    return &(*it);
}

std::string ObjectReader::fieldContext(const char* key) const { return ctx_ + "." + key; }

void ObjectReader::err(const char* key, const std::string& message) {
    report_.add(source_, fieldContext(key), message);
}

std::string ObjectReader::reqString(const char* key) {
    const Json* p = find(key);
    if (p == nullptr) {
        err(key, "missing required string");
        return "";
    }
    if (!p->is_string()) {
        err(key, "expected string");
        return "";
    }
    return p->get<std::string>();
}

std::string ObjectReader::optString(const char* key, const std::string& fallback) {
    const Json* p = find(key);
    if (p == nullptr) {
        return fallback;
    }
    if (!p->is_string()) {
        err(key, "expected string");
        return fallback;
    }
    return p->get<std::string>();
}

int ObjectReader::reqInt(const char* key) {
    const Json* p = find(key);
    if (p == nullptr) {
        err(key, "missing required integer");
        return 0;
    }
    if (!p->is_number_integer()) {
        err(key, "expected integer");
        return 0;
    }
    return p->get<int>();
}

int ObjectReader::reqIntMin(const char* key, int minValue) {
    const Json* p = find(key);
    if (p == nullptr) {
        err(key, "missing required integer >= " + std::to_string(minValue));
        return minValue;
    }
    if (!p->is_number_integer()) {
        err(key, "expected integer");
        return minValue;
    }
    const int value = p->get<int>();
    if (value < minValue) {
        err(key, "expected integer >= " + std::to_string(minValue) + ", got " +
                     std::to_string(value));
        return minValue;
    }
    return value;
}

int ObjectReader::reqIntRange(const char* key, int minValue, int maxValue) {
    const int value = reqIntMin(key, minValue);
    if (value > maxValue) {
        err(key, "expected integer <= " + std::to_string(maxValue) + ", got " +
                     std::to_string(value));
        return maxValue;
    }
    return value;
}

int ObjectReader::optInt(const char* key, int fallback) {
    const Json* p = find(key);
    if (p == nullptr) {
        return fallback;
    }
    if (!p->is_number_integer()) {
        err(key, "expected integer");
        return fallback;
    }
    return p->get<int>();
}

int ObjectReader::optIntMin(const char* key, int minValue, int fallback) {
    const Json* p = find(key);
    if (p == nullptr) {
        return fallback;
    }
    if (!p->is_number_integer()) {
        err(key, "expected integer");
        return fallback;
    }
    const int value = p->get<int>();
    if (value < minValue) {
        err(key, "expected integer >= " + std::to_string(minValue) + ", got " +
                     std::to_string(value));
        return minValue;
    }
    return value;
}

float ObjectReader::optFloat(const char* key, float fallback) {
    const Json* p = find(key);
    if (p == nullptr) {
        return fallback;
    }
    if (!p->is_number()) {
        err(key, "expected number");
        return fallback;
    }
    return p->get<float>();
}

bool ObjectReader::optBool(const char* key, bool fallback) {
    const Json* p = find(key);
    if (p == nullptr) {
        return fallback;
    }
    if (!p->is_boolean()) {
        err(key, "expected boolean");
        return fallback;
    }
    return p->get<bool>();
}

std::vector<std::string> ObjectReader::optStringArray(const char* key) {
    std::vector<std::string> out;
    const Json* p = find(key);
    if (p == nullptr) {
        return out;
    }
    if (!p->is_array()) {
        err(key, "expected array of strings");
        return out;
    }
    int index = 0;
    for (const auto& element : *p) {
        if (!element.is_string()) {
            err(key, "element [" + std::to_string(index) + "] is not a string");
        } else {
            out.push_back(element.get<std::string>());
        }
        ++index;
    }
    return out;
}

StatBlock ObjectReader::reqStatBlock(const char* key) {
    const Json* p = find(key);
    if (p == nullptr) {
        err(key, "missing required stat block");
        return {};
    }
    if (!p->is_object()) {
        err(key, "expected stat object");
        return {};
    }
    ObjectReader r(*p, fieldContext(key), source_, report_);
    StatBlock s;
    s.maxHp = r.reqIntMin("hp", 1);
    s.attack = r.reqIntMin("attack", 0);
    s.magic = r.reqIntMin("magic", 0);
    s.defense = r.reqIntMin("defense", 0);
    s.speed = r.reqIntMin("speed", 0);
    return s;
}

StatBlock ObjectReader::optStatBlock(const char* key) {
    const Json* p = find(key);
    if (p == nullptr) {
        return {};
    }
    if (!p->is_object()) {
        err(key, "expected stat object");
        return {};
    }
    ObjectReader r(*p, fieldContext(key), source_, report_);
    StatBlock s;
    s.maxHp = r.optInt("hp", 0);
    s.attack = r.optInt("attack", 0);
    s.magic = r.optInt("magic", 0);
    s.defense = r.optInt("defense", 0);
    s.speed = r.optInt("speed", 0);
    return s;
}

StatGrowth ObjectReader::optStatGrowth(const char* key) {
    const Json* p = find(key);
    if (p == nullptr) {
        return {};
    }
    if (!p->is_object()) {
        err(key, "expected growth object");
        return {};
    }
    ObjectReader r(*p, fieldContext(key), source_, report_);
    StatGrowth g;
    g.maxHp = r.optFloat("hp", 0.0f);
    g.attack = r.optFloat("attack", 0.0f);
    g.magic = r.optFloat("magic", 0.0f);
    g.defense = r.optFloat("defense", 0.0f);
    g.speed = r.optFloat("speed", 0.0f);
    return g;
}

}  // namespace cd::content
