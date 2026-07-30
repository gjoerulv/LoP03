#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "editor/CategoryDescriptors.hpp"

// CrystalForge's in-memory document store (M59). One ordered_json document per
// data file is THE source of truth while the editor runs: edits mutate the
// documents by key, unknown keys ride along untouched, and every save goes
// through the canonical writer + an atomic replace. The content-Def structs
// are never serialized back — validation re-runs the real loader over these
// documents instead. Pure std/nlohmann; no raylib.

namespace cd::editor {

struct DocFile {
    Category category = Category::Skills;
    std::filesystem::path path;
    OrderedJson root;  // the whole file document
    bool loaded = false;
    bool dirty = false;
    std::string loadError;  // non-empty when the file failed to parse/read
};

class EditorDocs {
public:
    // Parses every data file under `dataDir`. Returns false when any file
    // failed (the failure is recorded per file; the editor still opens the
    // rest — a malformed file must never take the tool down).
    bool loadAll(const std::filesystem::path& dataDir);

    const std::filesystem::path& dataDir() const { return dataDir_; }

    DocFile& file(Category category);
    const DocFile& file(Category category) const;

    // The entity array of a category (nullptr for Composition, whose "entity"
    // is the document root itself, and for a file that failed to load).
    OrderedJson* entities(Category category);
    const OrderedJson* entities(Category category) const;

    int entityCount(Category category) const;
    // The editable object for (category, index): an array element, or the
    // document root for Composition (index ignored). nullptr when absent.
    OrderedJson* entityAt(Category category, int index);
    const OrderedJson* entityAt(Category category, int index) const;

    // Row label ("iron_sword") and trailing column ("Iron Sword") for lists.
    std::string entityLabel(Category category, int index) const;
    std::string entitySuffix(Category category, int index) const;

    void markDirty(Category category);
    bool anyDirty() const;
    std::vector<Category> dirtyCategories() const;

    // Canonical + atomic save of one/all dirty files; false + `error` on the
    // first failure (remaining dirt is preserved).
    bool saveFile(Category category, std::string& error);
    bool saveAllDirty(std::string& error);

    // Serializes every loaded file canonically and writes it regardless of
    // dirt (the one-time normalization / drift repair). Returns the number of
    // files whose bytes changed; false + `error` on a write failure.
    bool canonicalizeAll(int& changedFiles, std::string& error);

    // Entity mutations (no-ops for Composition). All mark the file dirty.
    int addEntity(Category category);              // minimal skeleton; returns index (-1 = failed)
    int duplicateEntity(Category category, int index);  // "<id>_copy" (uniquified)
    bool removeEntity(Category category, int index);

    // How many times `"id"` appears as a JSON string anywhere OUTSIDE the
    // entity that defines it — the delete/rename guard's reference count.
    int referenceCount(const std::string& id, Category owner, int ownerIndex) const;

private:
    std::filesystem::path dataDir_;
    std::vector<DocFile> files_;  // one per categories() entry, same order
};

}  // namespace cd::editor
