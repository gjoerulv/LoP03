#pragma once

#include <cstddef>
#include <string>
#include <vector>

// M52 — the in-battle action log, as pure data.
//
// A fixed-capacity ring buffer of the exact message strings BattleState shows
// after each resolved action. It is presentation-only: owned by BattleState and
// freed with the battle, it never touches the battle model, its roll stream, or
// any scoring/state. Raylib-free by design (the QuitPrompt.hpp precedent), so it
// is unit-tested in the headless suite while BattleLogState stays a rendering
// state.

namespace cd {

class BattleLog {
public:
    // The most recent entries the log retains — a long fight's recent history
    // without unbounded growth.
    static constexpr std::size_t kMaxEntries = 30;

    // Appends one shown line, keeping only the last kMaxEntries (the oldest is
    // evicted when full). Empty lines are ignored (nothing was shown).
    void push(const std::string& line) {
        if (line.empty()) {
            return;
        }
        if (entries_.size() >= kMaxEntries) {
            entries_.erase(entries_.begin());
        }
        entries_.push_back(line);
    }

    // Oldest -> newest.
    const std::vector<std::string>& entries() const { return entries_; }
    bool empty() const { return entries_.empty(); }
    std::size_t size() const { return entries_.size(); }

private:
    std::vector<std::string> entries_;
};

}  // namespace cd
