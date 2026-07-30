#pragma once

#include <string>
#include <vector>

#include "content/ContentDatabase.hpp"
#include "content/LoadReport.hpp"
#include "editor/EditorDocs.hpp"

// CrystalForge validation (M59): the in-memory documents are serialized and
// pushed through the REAL content loader into a scratch database, then
// cross-reference-checked — the same code path the game runs at startup, so
// editor validation can never drift from what the game accepts. On top sits a
// tiny quick-sim battery: three fixed deterministic battles through the real
// battle::simulate that catch "you probably broke the difficulty curve"
// mistakes seconds after a save. Pure; no raylib.

namespace cd::editor {

struct QuickCheckResult {
    std::string name;
    bool ran = false;     // false: prerequisites missing (e.g. boss deleted)
    bool passed = false;  // meaningful only when ran
    std::string detail;   // "won in 12 turns, party HP 63%" / what was missing
};

struct ValidationResult {
    content::LoadReport report;      // loader + cross-reference errors
    bool databaseOk = false;         // report.ok() after the full load
    std::vector<QuickCheckResult> checks;  // empty when databaseOk is false
};

// Builds a scratch database from the documents through the real parsers.
// Returns report.ok().
bool buildDatabase(const EditorDocs& docs, content::ContentDatabase& db,
                   content::LoadReport& rep);

// Shared party-building vocabulary (quick checks + the M60 sim lab).
enum class GearTier { None, Median, Best };

// The four launch classes when present, else the first four non-King classes
// sorted by id. Never returns null entries.
std::vector<const content::ClassDef*> defaultSimClasses(const content::ContentDatabase& db);

// A deterministic equipment pick for a slot at a tier; weapons favor the
// class's better attack stat. "" when nothing fits (or tier None / banned).
std::string pickSimGear(const content::ContentDatabase& db, const content::ClassDef& cls,
                        content::EquipSlot slot, GearTier tier);

// Full pass: loader + references + (only when the database is clean) the
// quick-sim battery.
ValidationResult validateDocs(const EditorDocs& docs);

// The battery alone (exposed for the shell's F5 and for tests).
std::vector<QuickCheckResult> runQuickChecks(const content::ContentDatabase& db);

}  // namespace cd::editor
