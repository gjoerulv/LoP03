#include "editor/EditorDocs.hpp"

#include <algorithm>
#include <fstream>
#include <sstream>

#include "platform/AtomicFile.hpp"

namespace cd::editor {

namespace {

std::string readWholeFile(const std::filesystem::path& path, std::string& error) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        error = "cannot open " + path.string();
        return {};
    }
    std::ostringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

// A minimal new entity: every required field at a sensible starting value
// (descriptor defaults; required enums get their default id).
OrderedJson skeletonFor(Category category, const std::string& newId) {
    OrderedJson entity = OrderedJson::object();
    bool hasNameField = false;
    for (const FieldDesc& desc : descriptorsFor(category)) {
        if (desc.key == "name") {
            hasNameField = true;
        }
        if (!desc.required) {
            continue;
        }
        if (desc.isId) {
            entity[desc.key] = newId;
            continue;
        }
        if (desc.kind == FieldKind::Object) {
            OrderedJson child = OrderedJson::object();
            for (const FieldDesc& sub : desc.children) {
                child[sub.key] = fieldValue(child, sub);
            }
            entity[desc.key] = child;
            continue;
        }
        entity[desc.key] = fieldValue(entity, desc);
    }
    // Only categories that actually have a name field get the placeholder —
    // injecting one elsewhere (story, event flavor, curio lore) would save a
    // stray unrecognized key (M86 fix).
    if (hasNameField && entity.find("name") == entity.end()) {
        entity["name"] = "New Entry";
    }
    return entity;
}

}  // namespace

bool EditorDocs::loadAll(const std::filesystem::path& dataDir) {
    dataDir_ = dataDir;
    files_.clear();
    bool allOk = true;
    for (const CategoryInfo& info : categories()) {
        DocFile doc;
        doc.category = info.category;
        doc.path = dataDir / info.filename;
        std::string error;
        const std::string text = readWholeFile(doc.path, error);
        if (!error.empty()) {
            doc.loadError = error;
            allOk = false;
        } else {
            doc.root = OrderedJson::parse(text, nullptr, false);
            if (doc.root.is_discarded() || !doc.root.is_object()) {
                doc.loadError = info.filename + std::string(": not a valid JSON object");
                doc.root = OrderedJson::object();
                allOk = false;
            } else {
                doc.loaded = true;
            }
        }
        files_.push_back(std::move(doc));
    }
    return allOk;
}

DocFile& EditorDocs::file(Category category) {
    return files_[static_cast<std::size_t>(category)];
}

const DocFile& EditorDocs::file(Category category) const {
    return files_[static_cast<std::size_t>(category)];
}

OrderedJson* EditorDocs::entities(Category category) {
    DocFile& doc = file(category);
    const CategoryInfo& info = infoFor(category);
    if (!doc.loaded || info.arrayKey[0] == '\0') {
        return nullptr;
    }
    const auto it = doc.root.find(info.arrayKey);
    if (it == doc.root.end() || !it->is_array()) {
        return nullptr;
    }
    return &*it;
}

const OrderedJson* EditorDocs::entities(Category category) const {
    return const_cast<EditorDocs*>(this)->entities(category);
}

int EditorDocs::entityCount(Category category) const {
    if (category == Category::Composition) {
        return file(category).loaded ? 1 : 0;
    }
    const OrderedJson* arr = entities(category);
    return arr != nullptr ? static_cast<int>(arr->size()) : 0;
}

OrderedJson* EditorDocs::entityAt(Category category, int index) {
    if (category == Category::Composition) {
        DocFile& doc = file(category);
        return doc.loaded ? &doc.root : nullptr;
    }
    OrderedJson* arr = entities(category);
    if (arr == nullptr || index < 0 || index >= static_cast<int>(arr->size())) {
        return nullptr;
    }
    return &(*arr)[static_cast<std::size_t>(index)];
}

const OrderedJson* EditorDocs::entityAt(Category category, int index) const {
    return const_cast<EditorDocs*>(this)->entityAt(category, index);
}

std::string EditorDocs::entityLabel(Category category, int index) const {
    const OrderedJson* entity = entityAt(category, index);
    if (entity == nullptr || !entity->is_object()) {
        return "?";
    }
    if (category == Category::Composition) {
        return "composition";
    }
    if (category == Category::Story) {
        const auto town = entity->find("town");
        return "town " + (town != entity->end() ? town->dump() : std::string("?"));
    }
    const auto id = entity->find("id");
    return id != entity->end() && id->is_string() ? id->get<std::string>() : "(no id)";
}

std::string EditorDocs::entitySuffix(Category category, int index) const {
    const OrderedJson* entity = entityAt(category, index);
    if (entity == nullptr || !entity->is_object()) {
        return {};
    }
    if (category == Category::Composition) {
        return {};
    }
    // Story and event flavor label by title; curio lore has neither name nor
    // title, so its lookup misses and the suffix stays empty (ids carry it).
    const char* key = category == Category::Story || category == Category::EventFlavor
                          ? "title"
                          : "name";
    const auto it = entity->find(key);
    return it != entity->end() && it->is_string() ? it->get<std::string>() : std::string();
}

void EditorDocs::markDirty(Category category) { file(category).dirty = true; }

bool EditorDocs::anyDirty() const {
    for (const DocFile& doc : files_) {
        if (doc.dirty) {
            return true;
        }
    }
    return false;
}

std::vector<Category> EditorDocs::dirtyCategories() const {
    std::vector<Category> out;
    for (const DocFile& doc : files_) {
        if (doc.dirty) {
            out.push_back(doc.category);
        }
    }
    return out;
}

bool EditorDocs::saveFile(Category category, std::string& error) {
    DocFile& doc = file(category);
    if (!doc.loaded) {
        error = doc.path.filename().string() + " was never loaded";
        return false;
    }
    const std::string text =
        canonicalize(doc.root, styleForFile(doc.path.filename().string()));
    if (!platform::writeTextFileAtomically(doc.path, text, error)) {
        return false;
    }
    doc.dirty = false;
    return true;
}

bool EditorDocs::saveAllDirty(std::string& error) {
    for (DocFile& doc : files_) {
        if (doc.dirty && !saveFile(doc.category, error)) {
            return false;
        }
    }
    return true;
}

bool EditorDocs::canonicalizeAll(int& changedFiles, std::string& error) {
    changedFiles = 0;
    for (DocFile& doc : files_) {
        if (!doc.loaded) {
            continue;
        }
        const std::string text =
            canonicalize(doc.root, styleForFile(doc.path.filename().string()));
        std::string readError;
        std::string before = readWholeFile(doc.path, readError);
        // Line-ending tolerant: a CRLF checkout (git autocrlf) is not content
        // drift, so it neither counts as changed nor gets fought over.
        before.erase(std::remove(before.begin(), before.end(), '\r'), before.end());
        if (readError.empty() && before == text) {
            continue;
        }
        if (!platform::writeTextFileAtomically(doc.path, text, error)) {
            return false;
        }
        ++changedFiles;
        doc.dirty = false;
    }
    return true;
}

int EditorDocs::addEntity(Category category) {
    OrderedJson* arr = entities(category);
    if (arr == nullptr) {
        return -1;
    }
    // A unique fresh id within this category.
    const CategoryInfo& info = infoFor(category);
    std::string base = std::string("new_") + info.arrayKey;
    if (!base.empty() && base.back() == 's') {
        base.pop_back();  // "new_skills" -> "new_skill"
    }
    std::string id = base;
    for (int n = 2; referenceCount(id, category, -1) > 0 || [&] {
             for (const OrderedJson& el : *arr) {
                 const auto it = el.find("id");
                 if (it != el.end() && it->is_string() && it->get<std::string>() == id) {
                     return true;
                 }
             }
             return false;
         }();
         ++n) {
        id = base + "_" + std::to_string(n);
    }
    arr->push_back(skeletonFor(category, id));
    markDirty(category);
    return static_cast<int>(arr->size()) - 1;
}

int EditorDocs::duplicateEntity(Category category, int index) {
    OrderedJson* arr = entities(category);
    const OrderedJson* source = entityAt(category, index);
    if (arr == nullptr || source == nullptr) {
        return -1;
    }
    OrderedJson copy = *source;
    const auto idIt = copy.find("id");
    if (idIt != copy.end() && idIt->is_string()) {
        std::string id = idIt->get<std::string>() + "_copy";
        int n = 2;
        auto taken = [&](const std::string& candidate) {
            for (const OrderedJson& el : *arr) {
                const auto it = el.find("id");
                if (it != el.end() && it->is_string() && it->get<std::string>() == candidate) {
                    return true;
                }
            }
            return false;
        };
        while (taken(id)) {
            id = idIt->get<std::string>() + "_copy" + std::to_string(n++);
        }
        copy["id"] = id;
    }
    arr->insert(arr->begin() + index + 1, copy);
    markDirty(category);
    return index + 1;
}

bool EditorDocs::removeEntity(Category category, int index) {
    OrderedJson* arr = entities(category);
    if (arr == nullptr || index < 0 || index >= static_cast<int>(arr->size())) {
        return false;
    }
    arr->erase(arr->begin() + index);
    markDirty(category);
    return true;
}

int EditorDocs::referenceCount(const std::string& id, Category owner, int ownerIndex) const {
    if (id.empty()) {
        return 0;
    }
    const std::string needle = OrderedJson(id).dump();  // "\"id\"" with escaping
    int count = 0;
    for (const DocFile& doc : files_) {
        if (!doc.loaded) {
            continue;
        }
        std::string text;
        if (doc.category == owner && ownerIndex >= 0) {
            // Serialize the file WITHOUT the owning entity so its own id (and
            // self-references) do not count.
            OrderedJson clone = doc.root;
            const CategoryInfo& info = infoFor(doc.category);
            auto it = clone.find(info.arrayKey);
            if (it != clone.end() && it->is_array() &&
                ownerIndex < static_cast<int>(it->size())) {
                it->erase(it->begin() + ownerIndex);
            }
            text = clone.dump();
        } else {
            text = doc.root.dump();
        }
        for (std::size_t pos = text.find(needle); pos != std::string::npos;
             pos = text.find(needle, pos + needle.size())) {
            ++count;
        }
    }
    return count;
}

}  // namespace cd::editor
