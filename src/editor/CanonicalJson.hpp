#pragma once

#include <string>

#include <nlohmann/json.hpp>

// CrystalForge's canonical content-JSON writer (M59). The editor's documents
// are nlohmann::ordered_json (insertion-ordered keys — plain nlohmann::json
// sorts keys alphabetically and would churn every hand-authored file), and
// every save is serialized through exactly one formatter so the on-disk shape
// can never drift between saves. Pure string building; no filesystem, no
// raylib — unit-tested against the shipped files in tests/test_editor_canonical.cpp.

namespace cd::editor {

using OrderedJson = nlohmann::ordered_json;

// How a file's top-level entity array is laid out. The shipped files use two
// shapes: one ENTITY per line (skills/enemies/items/passives — grep-able rows,
// tight diffs) and pretty BLOCK entities (classes/bosses/themes/story —
// learnsets and long bodies would make unreadable single lines). composition
// has no entity array; BlockEntities renders its nested section objects.
enum class FileStyle {
    InlineEntities,  // each array element compact on one line
    BlockEntities,   // each array element multi-line, nested values inline
};

// Serializes a whole document root in the canonical shape (2-space indent,
// trailing newline). Deterministic: canonicalize(parse(canonicalize(x))) ==
// canonicalize(x) by construction.
std::string canonicalize(const OrderedJson& root, FileStyle style);

// The style for a shipped data file, by bare filename ("skills.json").
// Unknown filenames default to BlockEntities (the safe, readable shape).
FileStyle styleForFile(const std::string& filename);

}  // namespace cd::editor
