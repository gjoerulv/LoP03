#pragma once

#include <filesystem>
#include <string>

#include <nlohmann/json.hpp>

#include "content/ContentDatabase.hpp"
#include "content/LoadReport.hpp"

// Loading and validating content. Every entry point is defensive: malformed or
// missing input produces reported errors, never an exception to the caller and
// never a crash.

namespace cd::content {

using Json = nlohmann::json;

inline constexpr int kContentSchemaVersion = 1;

// In-memory parsers (unit-testable with JSON literals). Each expects a top-level
// object: { "version": <int>, "<plural>": [ {...}, ... ] }. Valid entries are
// added to `db`; problems are appended to `rep`. `source` labels error messages.
void parseSkills(const Json& root, const std::string& source, ContentDatabase& db, LoadReport& rep);
void parseClasses(const Json& root, const std::string& source, ContentDatabase& db, LoadReport& rep);
void parseEnemies(const Json& root, const std::string& source, ContentDatabase& db, LoadReport& rep);
void parseItems(const Json& root, const std::string& source, ContentDatabase& db, LoadReport& rep);
void parseBosses(const Json& root, const std::string& source, ContentDatabase& db, LoadReport& rep);
void parseThemes(const Json& root, const std::string& source, ContentDatabase& db, LoadReport& rep);
void parsePassives(const Json& root, const std::string& source, ContentDatabase& db,
                   LoadReport& rep);
void parseMilestones(const Json& root, const std::string& source, ContentDatabase& db,
                     LoadReport& rep);  // M63
// M59 (CrystalForge): exported so the editor can validate in-memory documents
// through the exact loader path. Both existed with external linkage since M41 /
// M20 (loadAll calls them); this only declares them.
void parseStory(const Json& root, const std::string& source, ContentDatabase& db, LoadReport& rep);
void parseComposition(const Json& root, const std::string& source, ContentDatabase& db,
                      LoadReport& rep);
void parseEventFlavor(const Json& root, const std::string& source, ContentDatabase& db,
                      LoadReport& rep);  // M80
void parseCurioLore(const Json& root, const std::string& source, ContentDatabase& db,
                    LoadReport& rep);  // M85 (declared in M86 for the editor, the M59 precedent)
void parseCutscenes(const Json& root, const std::string& source, ContentDatabase& db,
                    LoadReport& rep);  // M97
void parseTutorialTexts(const Json& root, const std::string& source, ContentDatabase& db,
                        LoadReport& rep);
// M112: the Jester's lore questions (optional file, see LoreQuestionDef).
void parseLoreQuestions(const Json& root, const std::string& source, ContentDatabase& db,
                        LoadReport& rep);  // M99

// Cross-reference checks: skill ids referenced by classes, enemies, and scrolls
// must exist in `db`.
void validateReferences(const ContentDatabase& db, LoadReport& rep);

// Reads and parses a JSON file, catching syntax errors (no exceptions escape).
bool readJsonFile(const std::filesystem::path& file, Json& out, LoadReport& rep);

// Loads skills/classes/enemies/items from `dataRoot` and validates references.
// Returns true only if every file parsed and zero errors were reported.
bool loadAll(const std::filesystem::path& dataRoot, ContentDatabase& db, LoadReport& rep);

}  // namespace cd::content
