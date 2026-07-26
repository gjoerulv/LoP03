#pragma once

#include <filesystem>
#include <string>
#include <vector>

// M60 — the per-category test runner's pure parts: the category -> Catch2
// filter mapping and the invocation builder. The shell owns the actual
// platform::ProcessRunner; everything here is testable headlessly.
//
// Filters use Catch2's `--filenames-as-tags` (verified present in the pinned
// v3.15.1): every TEST_CASE carries a [#test_<file>] tag, so categories can
// name whole files without touching the test sources.

namespace cd::editor {

struct TestCategory {
    std::string name;                    // "Enemies & Bosses"
    std::vector<std::string> testFiles;  // "test_balance", ... (no extension)
};

// The category table (last entry = "Everything", empty file list = no filter).
const std::vector<TestCategory>& testCategories();

// "[#test_balance],[#test_danger]" (comma = OR in a Catch2 test spec);
// "" for an empty file list (run all).
std::string catch2Spec(const TestCategory& category);

// The full argument list for crystal_tests.exe.
std::vector<std::string> testInvocation(const TestCategory& category);

// Where crystal_tests.exe lives: beside the running editor binary.
std::filesystem::path testBinaryPath(const std::filesystem::path& editorExeDir);

}  // namespace cd::editor
