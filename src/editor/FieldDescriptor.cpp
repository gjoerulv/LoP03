#include "editor/FieldDescriptor.hpp"

#include <cmath>

namespace cd::editor {

namespace {

OrderedJson defaultFor(const FieldDesc& desc) {
    switch (desc.kind) {
        case FieldKind::Int:
            return OrderedJson(static_cast<long long>(std::llround(desc.defaultNumber)));
        case FieldKind::Float:
            return OrderedJson(desc.defaultNumber);
        case FieldKind::Bool:
            return OrderedJson(desc.defaultBool);
        case FieldKind::Enum:
            return OrderedJson(desc.defaultString);
        case FieldKind::String:
        case FieldKind::Text:
        case FieldKind::IdRef:
            return OrderedJson(desc.defaultString);
        case FieldKind::IdList:
        case FieldKind::EnumList:
        case FieldKind::ObjectArray:
            return OrderedJson::array();
        case FieldKind::Object:
            return OrderedJson::object();
    }
    return OrderedJson();
}

}  // namespace

OrderedJson fieldValue(const OrderedJson& entity, const FieldDesc& desc) {
    if (entity.is_object()) {
        const auto it = entity.find(desc.key);
        if (it != entity.end()) {
            return *it;
        }
    }
    return defaultFor(desc);
}

bool isDefaultValue(const FieldDesc& desc, const OrderedJson& value) {
    switch (desc.kind) {
        case FieldKind::Int:
            return value.is_number() &&
                   value.get<long long>() == std::llround(desc.defaultNumber);
        case FieldKind::Float:
            return value.is_number() && value.get<double>() == desc.defaultNumber;
        case FieldKind::Bool:
            return value.is_boolean() && value.get<bool>() == desc.defaultBool;
        case FieldKind::Enum:
        case FieldKind::String:
        case FieldKind::Text:
        case FieldKind::IdRef:
            return value.is_string() && value.get<std::string>() == desc.defaultString;
        case FieldKind::IdList:
        case FieldKind::EnumList:
        case FieldKind::ObjectArray:
            return value.is_array() && value.empty();
        case FieldKind::Object: {
            if (!value.is_object()) {
                return false;
            }
            if (value.empty()) {
                return true;
            }
            // Every present child at ITS default (an absent child already is).
            for (const FieldDesc& child : desc.children) {
                const auto it = value.find(child.key);
                if (it != value.end() && !isDefaultValue(child, *it)) {
                    return false;
                }
            }
            return true;
        }
    }
    return false;
}

void setFieldValue(OrderedJson& entity, const FieldDesc& desc, const OrderedJson& value) {
    if (!entity.is_object()) {
        return;
    }
    if (!desc.required && isDefaultValue(desc, value)) {
        entity.erase(desc.key);
        return;
    }
    entity[desc.key] = value;
}

OrderedJson childValue(const OrderedJson& entity, const FieldDesc& parent, const FieldDesc& child) {
    if (entity.is_object()) {
        const auto it = entity.find(parent.key);
        if (it != entity.end() && it->is_object()) {
            return fieldValue(*it, child);
        }
    }
    return fieldValue(OrderedJson::object(), child);
}

void setChildValue(OrderedJson& entity, const FieldDesc& parent, const FieldDesc& child,
                   const OrderedJson& value) {
    if (!entity.is_object()) {
        return;
    }
    OrderedJson parentValue = OrderedJson::object();
    const auto it = entity.find(parent.key);
    if (it != entity.end() && it->is_object()) {
        parentValue = *it;
    }
    // Children are always "required" inside their parent when non-default;
    // the sparse policy is applied at the parent level below.
    if (isDefaultValue(child, value)) {
        parentValue.erase(child.key);
    } else {
        parentValue[child.key] = value;
    }
    setFieldValue(entity, parent, parentValue);
}

std::vector<std::string> unrecognizedKeys(const OrderedJson& entity,
                                          const std::vector<FieldDesc>& descs) {
    std::vector<std::string> unknown;
    if (!entity.is_object()) {
        return unknown;
    }
    for (auto it = entity.begin(); it != entity.end(); ++it) {
        bool covered = false;
        for (const FieldDesc& desc : descs) {
            if (desc.key == it.key()) {
                covered = true;
                break;
            }
        }
        if (!covered) {
            unknown.push_back(it.key());
        }
    }
    return unknown;
}

}  // namespace cd::editor
