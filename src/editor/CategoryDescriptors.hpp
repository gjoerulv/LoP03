#pragma once

#include <string>
#include <vector>

#include "editor/FieldDescriptor.hpp"

// CrystalForge's per-category schema tables (M59): which file each category
// lives in and one FieldDesc per key the content loader reads. Completeness is
// pinned by tests/test_editor_descriptors.cpp against the shipped data files —
// a new content key without a descriptor fails the suite, so the editor can
// never silently hide a field. Pure data; no raylib.

namespace cd::editor {

struct CategoryInfo {
    Category category = Category::Skills;
    const char* filename = "";  // bare name under data/
    const char* arrayKey = "";  // top-level entity array ("" = composition's nested sections)
    const char* title = "";     // sidebar display
    bool hasIdKey = true;       // false: story (keyed by town) and composition
};

const std::vector<CategoryInfo>& categories();
const CategoryInfo& infoFor(Category category);

// The field descriptors for one category's entities (for Composition: the
// root object's sections). Stable references; built once.
const std::vector<FieldDesc>& descriptorsFor(Category category);

}  // namespace cd::editor
