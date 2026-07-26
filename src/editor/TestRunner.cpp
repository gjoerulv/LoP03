#include "editor/TestRunner.hpp"

namespace cd::editor {

const std::vector<TestCategory>& testCategories() {
    // Grouped by what a designer just edited; file names per the tests/ dir.
    static const std::vector<TestCategory> kCategories = {
        {"Enemies & bosses",
         {"test_balance", "test_danger", "test_composition", "test_boss_mechanics",
          "test_kings_court"}},
        {"Skills & status",
         {"test_elements", "test_status", "test_status_v2", "test_battle",
          "test_battle_rules_v4", "test_rules_v7_flow", "test_learnset"}},
        {"Items & economy",
         {"test_equipment", "test_equip_shop_filter", "test_economy", "test_royal_relics",
          "test_inventory"}},
        {"Classes & passives", {"test_passives", "test_kings_classes", "test_party",
                                "test_leveling"}},
        {"Content validation", {"test_content_loader", "test_content_validation",
                                "test_editor_canonical", "test_editor_descriptors"}},
        {"Everything", {}},
    };
    return kCategories;
}

std::string catch2Spec(const TestCategory& category) {
    std::string spec;
    for (const std::string& file : category.testFiles) {
        if (!spec.empty()) {
            spec += ",";
        }
        spec += "[#" + file + "]";
    }
    return spec;
}

std::vector<std::string> testInvocation(const TestCategory& category) {
    std::vector<std::string> args;
    const std::string spec = catch2Spec(category);
    if (!spec.empty()) {
        args.push_back(spec);
    }
    args.push_back("--filenames-as-tags");
    return args;
}

std::filesystem::path testBinaryPath(const std::filesystem::path& editorExeDir) {
    return editorExeDir / "crystal_tests.exe";
}

}  // namespace cd::editor
