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

// Cross-reference checks: skill ids referenced by classes, enemies, and scrolls
// must exist in `db`.
void validateReferences(const ContentDatabase& db, LoadReport& rep);

// Reads and parses a JSON file, catching syntax errors (no exceptions escape).
bool readJsonFile(const std::filesystem::path& file, Json& out, LoadReport& rep);

// Loads skills/classes/enemies/items from `dataRoot` and validates references.
// Returns true only if every file parsed and zero errors were reported.
bool loadAll(const std::filesystem::path& dataRoot, ContentDatabase& db, LoadReport& rep);

}  // namespace cd::content
