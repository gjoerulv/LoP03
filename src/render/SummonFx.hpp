#pragma once

#include <optional>
#include <string>

namespace cd {
class ResourceManager;
namespace render {

// M107 (owner request): the summons' stagecraft. When one of the three
// legends answers a call, its apparition fills the battlefield CENTER for the
// resolution beat — procedurally drawn (the ElementFx idiom: deterministic,
// palette-true, no new asset pipeline), with the Mighty G. Goose wearing the
// real goose sprite writ large. `t` runs 1 -> 0 across the beat; the flash
// gate collapses the burst work to the creature alone (M18 settings).

enum class SummonKind { Goose, Sentinel, Spring };

// The apparition a summon skill conjures; nullopt for every other skill.
std::optional<SummonKind> summonKindFor(const std::string& skillId);

void drawSummonApparition(ResourceManager& resources, SummonKind kind, int centerX,
                          int centerY, float t, bool flashEnabled);

}  // namespace render
}  // namespace cd
