#pragma once

// M47 — one builder for the pause menus' three-answer quit prompt, so the town
// menu, the dungeon menu, and the capture scenes all raise the identical
// question. The answer strings and indices are pure data in QuitPrompt.hpp; this
// header only binds them to the actions, which need the state stack.

namespace cd {

class StateStack;
struct AppContext;

// Pushes the Quit prompt: Quit to Title / Quit Game / Keep Playing, cursor on
// Keep Playing. `body` is quit::kTownBody or quit::kDungeonBody — the warning
// must state what THIS screen actually loses.
void pushQuitPrompt(StateStack& stack, AppContext& context, const char* body);

}  // namespace cd
