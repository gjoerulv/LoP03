// M59 — CrystalForge descriptor coverage and write policy. The completeness
// sweep is the editor's staleness guard: every key that appears anywhere in
// the shipped data files (including keys nested in Object fields and
// ObjectArray elements) must be covered by a descriptor of its category, so a
// future content key without one fails the suite instead of silently hiding a
// field from designers.

#include <catch2/catch_test_macros.hpp>

#include <filesystem>

#include "editor/CategoryDescriptors.hpp"
#include "editor/EditorDocs.hpp"

namespace {

namespace fs = std::filesystem;
using cd::editor::Category;
using cd::editor::FieldDesc;
using cd::editor::FieldKind;
using cd::editor::OrderedJson;

const FieldDesc* descForKey(const std::vector<FieldDesc>& descs, const std::string& key) {
    for (const FieldDesc& desc : descs) {
        if (desc.key == key) {
            return &desc;
        }
    }
    return nullptr;
}

// Asserts every key of `entity` has a descriptor, recursing into Object
// children and ObjectArray elements.
void requireCovered(const OrderedJson& entity, const std::vector<FieldDesc>& descs,
                    const std::string& where) {
    for (auto it = entity.begin(); it != entity.end(); ++it) {
        INFO(where + "." + it.key());
        const FieldDesc* desc = descForKey(descs, it.key());
        REQUIRE(desc != nullptr);
        if (desc->kind == FieldKind::Object && it->is_object()) {
            requireCovered(*it, desc->children, where + "." + it.key());
        }
        if (desc->kind == FieldKind::ObjectArray && it->is_array()) {
            for (const OrderedJson& el : *it) {
                requireCovered(el, desc->children, where + "." + it.key() + "[]");
            }
        }
    }
}

}  // namespace

TEST_CASE("editor: descriptors cover every key in the shipped data", "[editor]") {
    cd::editor::EditorDocs docs;
    REQUIRE(docs.loadAll(fs::path(CRYSTAL_TEST_DATA_DIR)));
    for (const cd::editor::CategoryInfo& info : cd::editor::categories()) {
        const std::vector<FieldDesc>& descs = cd::editor::descriptorsFor(info.category);
        if (info.category == Category::Composition) {
            requireCovered(docs.file(info.category).root, descs, info.filename);
            continue;
        }
        const int count = docs.entityCount(info.category);
        REQUIRE(count > 0);
        for (int i = 0; i < count; ++i) {
            const OrderedJson* entity = docs.entityAt(info.category, i);
            REQUIRE(entity != nullptr);
            // "version" lives on the root, not on entities; every entity key
            // must be a real descriptor.
            requireCovered(*entity, descs, std::string(info.filename) + "[" +
                                               std::to_string(i) + "]");
        }
    }
}

TEST_CASE("editor: optional fields are omitted at their defaults", "[editor]") {
    const std::vector<FieldDesc>& descs = cd::editor::descriptorsFor(Category::Skills);
    const FieldDesc* power = descForKey(descs, "power");
    const FieldDesc* element = descForKey(descs, "element");
    const FieldDesc* also = descForKey(descs, "alsoBuffsEnemies");
    REQUIRE(power != nullptr);
    REQUIRE(element != nullptr);
    REQUIRE(also != nullptr);

    OrderedJson entity = OrderedJson::object();
    entity["id"] = "test";

    // Setting a non-default writes the key; back to default removes it.
    cd::editor::setFieldValue(entity, *power, OrderedJson(12));
    REQUIRE(entity.contains("power"));
    cd::editor::setFieldValue(entity, *power, OrderedJson(0));
    REQUIRE_FALSE(entity.contains("power"));

    cd::editor::setFieldValue(entity, *element, OrderedJson("fire"));
    REQUIRE(entity.contains("element"));
    cd::editor::setFieldValue(entity, *element, OrderedJson("none"));
    REQUIRE_FALSE(entity.contains("element"));

    cd::editor::setFieldValue(entity, *also, OrderedJson(true));
    REQUIRE(entity.contains("alsoBuffsEnemies"));
    cd::editor::setFieldValue(entity, *also, OrderedJson(false));
    REQUIRE_FALSE(entity.contains("alsoBuffsEnemies"));

    // Required fields are written even at a default-looking value.
    const FieldDesc* name = descForKey(descs, "name");
    REQUIRE(name != nullptr);
    cd::editor::setFieldValue(entity, *name, OrderedJson(""));
    REQUIRE(entity.contains("name"));
}

TEST_CASE("editor: object children collapse their parent at defaults", "[editor]") {
    const std::vector<FieldDesc>& descs = cd::editor::descriptorsFor(Category::Items);
    const FieldDesc* bonus = descForKey(descs, "statBonus");
    REQUIRE(bonus != nullptr);
    REQUIRE(bonus->kind == FieldKind::Object);
    const FieldDesc* attack = descForKey(bonus->children, "attack");
    REQUIRE(attack != nullptr);

    OrderedJson entity = OrderedJson::object();
    entity["id"] = "test";
    cd::editor::setChildValue(entity, *bonus, *attack, OrderedJson(6));
    REQUIRE(entity.contains("statBonus"));
    REQUIRE(entity["statBonus"]["attack"] == 6);
    cd::editor::setChildValue(entity, *bonus, *attack, OrderedJson(0));
    REQUIRE_FALSE(entity.contains("statBonus"));
}

TEST_CASE("editor: fieldValue falls back to the loader's defaults", "[editor]") {
    const std::vector<FieldDesc>& descs = cd::editor::descriptorsFor(Category::Enemies);
    const FieldDesc* minTown = descForKey(descs, "minTown");
    const FieldDesc* tier = descForKey(descs, "tier");
    REQUIRE(minTown != nullptr);
    REQUIRE(tier != nullptr);
    const OrderedJson entity = OrderedJson::object();
    REQUIRE(cd::editor::fieldValue(entity, *minTown) == 1);
    REQUIRE(cd::editor::fieldValue(entity, *tier) == "normal");
}

TEST_CASE("editor: unrecognized keys are reported, not lost", "[editor]") {
    const std::vector<FieldDesc>& descs = cd::editor::descriptorsFor(Category::Skills);
    OrderedJson entity = OrderedJson::object();
    entity["id"] = "test";
    entity["futureKey"] = 42;
    const std::vector<std::string> unknown = cd::editor::unrecognizedKeys(entity, descs);
    REQUIRE(unknown.size() == 1);
    REQUIRE(unknown.front() == "futureKey");
}
