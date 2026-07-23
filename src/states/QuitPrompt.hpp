#pragma once

#include <array>
#include <string>
#include <vector>

// M47 — the pause menus' quit question, as pure data.
//
// The town and dungeon pause menus and the capture scenes all raise the same
// three-answer prompt, so the labels, their order, the safe row, and the two
// warning bodies live here exactly once. Raylib-free by design (the
// EquipShopFilter/ItemShopFilter precedent), so the model is unit-testable in
// the headless suite while ConfirmPromptState stays a rendering state.

namespace cd::quit {

// Answer order, most-destructive first — the ConfirmPromptState convention, and
// what puts "Keep Playing" at the bottom where the cursor starts.
inline constexpr int kQuitToTitle = 0;
inline constexpr int kQuitGame = 1;
inline constexpr int kKeepPlaying = 2;
inline constexpr int kAnswerCount = 3;

// The row the cursor starts on and the row Cancel resolves to: never a
// destructive one.
inline constexpr int kSafeAnswer = kKeepPlaying;

inline constexpr const char* kTitle = "Quit";

// Quitting from town loses only what has happened since the last save.
inline constexpr const char* kTownBody = "Progress since your last save will be lost.";

// Quitting mid-run additionally throws the run away, but the dungeon-entry
// autosave is real and worth saying out loud.
inline constexpr const char* kDungeonBody =
    "This run will be lost. Progress since the autosave at dungeon entry is kept.";

inline std::array<const char*, kAnswerCount> answerLabels() {
    return {"Quit to Title", "Quit Game", "Keep Playing"};
}

inline std::vector<std::string> answerLabelList() {
    const std::array<const char*, kAnswerCount> labels = answerLabels();
    return {labels.begin(), labels.end()};
}

}  // namespace cd::quit
