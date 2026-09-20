#pragma once

// M126 (owner request 2026-09-20): what an outcome panel lists as RECEIVED.
// One line of gold - the total, however many times gold was handed over -
// and one line per distinct item, a repeated item counted ("Power Ring x2")
// instead of listed twice. Pure: the panel turns the rows into icon + text
// (gold in the body white, everything else in the reward gold); the tests
// pin the merging and the wording.

#include <string>
#include <vector>

#include "content/ContentDatabase.hpp"
#include "content/Definitions.hpp"

namespace cd {

struct LootRow {
    bool isGold = false;
    std::string itemId;  // empty on the gold row
    std::string icon;    // M81 gear icon texture id; empty = no icon
    std::string name;    // the display name, without the count
    int count = 0;       // pieces - or the amount, on the gold row

    // "26 gold" / "Power Ring" / "Power Ring x2".
    std::string text() const {
        if (isGold) {
            return std::to_string(count) + " gold";
        }
        return count > 1 ? name + " x" + std::to_string(count) : name;
    }
};

class LootSummary {
public:
    // Gold is totalled; nothing (or a loss) adds no row.
    void addGold(int amount) {
        if (amount > 0) {
            gold_ += amount;
        }
    }

    // A piece already listed is counted up; a new one joins in the order it
    // was received. An id the content does not know keeps its id as the name
    // (placeholder discipline - the row never vanishes).
    void addItem(const std::string& itemId, const content::ContentDatabase& db, int count = 1) {
        if (itemId.empty() || count <= 0) {
            return;
        }
        for (LootRow& r : items_) {
            if (r.itemId == itemId) {
                r.count += count;
                return;
            }
        }
        LootRow r;
        r.itemId = itemId;
        r.name = itemId;
        if (const content::ItemDef* it = db.findItem(itemId)) {
            r.name = it->name;
            r.icon = content::gearIconTextureId(*it);
        }
        r.count = count;
        items_.push_back(std::move(r));
    }

    void clear() {
        gold_ = 0;
        items_.clear();
    }

    bool empty() const { return gold_ <= 0 && items_.empty(); }
    int gold() const { return gold_; }

    // The gold line first (when any gold came), then the items.
    std::vector<LootRow> rows() const {
        std::vector<LootRow> out;
        out.reserve(items_.size() + 1);
        if (gold_ > 0) {
            LootRow g;
            g.isGold = true;
            g.name = "gold";
            g.count = gold_;
            out.push_back(std::move(g));
        }
        out.insert(out.end(), items_.begin(), items_.end());
        return out;
    }

private:
    int gold_ = 0;
    std::vector<LootRow> items_;
};

}  // namespace cd
