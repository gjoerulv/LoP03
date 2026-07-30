#pragma once

#include <string>
#include <vector>

#include "editor/CanonicalJson.hpp"

// CrystalForge's schema descriptors (M59). One FieldDesc per JSON key the
// loader reads; the tables in CategoryDescriptors.cpp drive both the form UI
// and the omit-when-default write policy. The descriptors never replace the
// loader as the validator — every edit is re-validated through the real
// content loader — they only describe what a form can offer. Pure; no raylib.

namespace cd::editor {

// Which data file a reference field points into (also the editor's category
// list). Composition is the one non-array file (nested section objects).
enum class Category {
    Skills,
    Classes,
    Enemies,
    Bosses,
    Items,
    Passives,
    Milestones,  // M63
    Themes,
    Composition,
    Story
};
inline constexpr int kCategoryCount = 9;

enum class FieldKind {
    String,      // short single-line text (names, roles)
    Text,        // long wrapped text (descriptions, story bodies)
    Int,         // stepped integer with bounds
    Float,       // stepped float (class growth curves)
    Bool,        // toggle
    Enum,        // one id from a fixed list (content enum ids)
    IdRef,       // one id referencing another category (may be empty)
    IdList,      // array of ids referencing another category
    EnumList,    // array of enum ids (tags, equip bans, affinity lists)
    Object,      // nested object edited as flattened child rows (statBonus)
    ObjectArray  // array of small objects (learnset, statuses) edited in a modal
};

struct FieldDesc {
    std::string key;    // JSON key (or sub-key inside Object/ObjectArray)
    std::string label;  // display label
    FieldKind kind = FieldKind::String;

    // Required fields are always written; optional fields are OMITTED from the
    // document when at their default, matching how the shipped files are
    // authored (absent key == default).
    bool required = false;
    bool isId = false;  // the entity's id key (rename warns about references)

    // Int/Float bounds and stepping (shared storage; Int uses whole numbers).
    double minValue = 0.0;
    double maxValue = 0.0;
    double step = 1.0;
    double defaultNumber = 0.0;

    bool defaultBool = false;
    std::string defaultString;  // Enum default id ("none"); String default ("")

    std::vector<std::string> enumValues;        // Enum/EnumList choices
    Category refCategory = Category::Skills;    // IdRef/IdList target
    std::vector<FieldDesc> children;            // Object / ObjectArray element fields
};

// Reads the field from an entity object, falling back to the descriptor's
// default when the key is absent (what the loader would do).
OrderedJson fieldValue(const OrderedJson& entity, const FieldDesc& desc);

// Writes the field, erasing the key instead when an optional field equals its
// default (empty string/list/object, default number/bool/enum) so documents
// keep the shipped files' sparse authoring style.
void setFieldValue(OrderedJson& entity, const FieldDesc& desc, const OrderedJson& value);

// True when `value` is the descriptor's default (the omit condition).
bool isDefaultValue(const FieldDesc& desc, const OrderedJson& value);

// Child access inside an Object field (e.g. statBonus.attack): reading falls
// back through a missing parent; writing creates the parent as needed and
// erases it again when every child is back at its default.
OrderedJson childValue(const OrderedJson& entity, const FieldDesc& parent, const FieldDesc& child);
void setChildValue(OrderedJson& entity, const FieldDesc& parent, const FieldDesc& child,
                   const OrderedJson& value);

// The keys of an entity that no descriptor in `descs` covers (surfaced in the
// form as read-only "(unrecognized key)" rows; they survive saves untouched).
std::vector<std::string> unrecognizedKeys(const OrderedJson& entity,
                                          const std::vector<FieldDesc>& descs);

}  // namespace cd::editor
