#include "save/SaveSystem.hpp"

#include <fstream>
#include <utility>

#include <nlohmann/json.hpp>

#include "content/ContentDatabase.hpp"
#include "content/ContentLoader.hpp"
#include "content/JsonValidation.hpp"
#include "game/Party.hpp"

namespace cd::save {

namespace fs = std::filesystem;
using content::Json;

const char* slotFileStem(SaveSlot slot) {
    switch (slot) {
        case SaveSlot::Auto: return "save_auto";
        case SaveSlot::Manual1: return "save_slot1";
        case SaveSlot::Manual2: return "save_slot2";
        case SaveSlot::Manual3: return "save_slot3";
    }
    return "save_slot1";
}

const char* slotDisplayName(SaveSlot slot) {
    switch (slot) {
        case SaveSlot::Auto: return "Autosave";
        case SaveSlot::Manual1: return "Slot 1";
        case SaveSlot::Manual2: return "Slot 2";
        case SaveSlot::Manual3: return "Slot 3";
    }
    return "Slot";
}

SaveSystem::SaveSystem(const content::ContentDatabase& db, fs::path directory)
    : db_(db), dir_(std::move(directory)) {}

fs::path SaveSystem::slotPath(SaveSlot slot) const {
    return dir_ / (std::string(slotFileStem(slot)) + ".json");
}

bool SaveSystem::exists(SaveSlot slot) const {
    std::error_code ec;
    return fs::exists(slotPath(slot), ec) && !ec;
}

bool SaveSystem::save(SaveSlot slot, const Party& party, content::LoadReport& report) const {
    const std::string source = slotPath(slot).filename().string();

    Json root;
    root["version"] = kSaveVersion;
    root["gold"] = party.gold;
    Json members = Json::array();
    for (const Character& c : party.members) {
        Json m;
        m["classId"] = c.classId;
        m["name"] = c.name;
        m["level"] = c.level;
        m["xp"] = c.xp;
        m["hp"] = c.hp;
        m["mp"] = c.mp;
        members.push_back(std::move(m));
    }
    root["party"] = std::move(members);

    Json items = Json::array();
    for (const ItemStack& stack : party.inventory.stacks) {
        Json js;
        js["itemId"] = stack.itemId;
        js["count"] = stack.count;
        items.push_back(std::move(js));
    }
    root["inventory"] = std::move(items);

    std::error_code ec;
    fs::create_directories(dir_, ec);
    if (ec) {
        report.add(source, "<file>", "could not create save directory: " + dir_.string());
        return false;
    }

    std::ofstream out(slotPath(slot), std::ios::binary | std::ios::trunc);
    if (!out) {
        report.add(source, "<file>", "could not open save file for writing");
        return false;
    }
    out << root.dump(2) << '\n';
    if (!out) {
        report.add(source, "<file>", "failed while writing save file");
        return false;
    }
    return true;
}

bool SaveSystem::autosave(const Party& party, content::LoadReport& report) const {
    return save(SaveSlot::Auto, party, report);
}

bool SaveSystem::load(SaveSlot slot, Party& outParty, content::LoadReport& report) const {
    const std::size_t before = report.errorCount();
    const std::string source = slotPath(slot).filename().string();

    Json root;
    if (!content::readJsonFile(slotPath(slot), root, report)) {
        return false;
    }
    if (!root.is_object()) {
        report.add(source, "<root>", "expected a top-level JSON object");
        return false;
    }

    content::ObjectReader rootReader(root, "save", source, report);
    const int version = rootReader.reqInt("version");
    if (version != kSaveVersion) {
        report.add(source, "version",
                   "unsupported save version " + std::to_string(version) + " (expected " +
                       std::to_string(kSaveVersion) + ")");
        return false;
    }

    Party loaded;
    loaded.gold = rootReader.optIntMin("gold", 0, 0);

    auto partyIt = root.find("party");
    if (partyIt == root.end() || !partyIt->is_array()) {
        report.add(source, "party", "expected array");
        return false;
    }

    int index = 0;
    for (const auto& element : *partyIt) {
        const std::string ctx = "party[" + std::to_string(index) + "]";
        ++index;
        if (!element.is_object()) {
            report.add(source, ctx, "expected object");
            continue;
        }
        const std::size_t elementBefore = report.errorCount();
        content::ObjectReader m(element, ctx, source, report);
        const std::string classId = m.reqString("classId");
        const std::string name = m.reqString("name");
        const int level = m.optIntMin("level", 1, 1);
        const int xp = m.optIntMin("xp", 0, 0);
        const int hp = m.optIntMin("hp", 0, 0);
        const int mp = m.optIntMin("mp", 0, 0);
        if (report.errorCount() != elementBefore) {
            continue;  // invalid member; skip
        }

        const content::ClassDef* cls = db_.findClass(classId);
        if (cls == nullptr) {
            report.add(source, ctx + ".classId", "unknown class '" + classId + "'");
            continue;
        }

        Character c = createCharacter(*cls, name, level);
        c.xp = xp;
        c.hp = hp;
        c.mp = mp;
        recomputeDerivedStats(c, *cls);  // clamps hp/mp to derived maxima
        loaded.members.push_back(std::move(c));
    }

    if (loaded.members.size() > kMaxPartySize) {
        report.add(source, "party",
                   "too many members (" + std::to_string(loaded.members.size()) + ", max " +
                       std::to_string(kMaxPartySize) + ")");
    }

    // Inventory is an optional, backward-compatible field (missing => empty).
    if (auto invIt = root.find("inventory"); invIt != root.end()) {
        if (!invIt->is_array()) {
            report.add(source, "inventory", "expected array");
        } else {
            int invIndex = 0;
            for (const auto& element : *invIt) {
                const std::string ctx = "inventory[" + std::to_string(invIndex) + "]";
                ++invIndex;
                if (!element.is_object()) {
                    report.add(source, ctx, "expected object");
                    continue;
                }
                const std::size_t elementBefore = report.errorCount();
                content::ObjectReader ir(element, ctx, source, report);
                const std::string itemId = ir.reqString("itemId");
                const int count = ir.optIntMin("count", 1, 1);
                if (report.errorCount() != elementBefore) {
                    continue;
                }
                if (db_.findItem(itemId) == nullptr) {
                    report.add(source, ctx + ".itemId", "unknown item '" + itemId + "'");
                    continue;
                }
                loaded.inventory.add(itemId, count);
            }
        }
    }

    if (report.errorCount() != before) {
        return false;  // leave outParty untouched on any problem
    }
    outParty = std::move(loaded);
    return true;
}

std::optional<SlotSummary> SaveSystem::summary(SaveSlot slot) const {
    Party tmp;
    content::LoadReport rep;
    if (!load(slot, tmp, rep)) {
        return std::nullopt;
    }
    SlotSummary s;
    s.partySize = static_cast<int>(tmp.size());
    s.highestLevel = highestLevel(tmp);
    s.gold = tmp.gold;
    return s;
}

}  // namespace cd::save
