#include "render/SummonFx.hpp"

#include <cmath>

#include "raylib.h"
#include "resource/ResourceManager.hpp"
#include "ui/UiStyle.hpp"

namespace cd::render {

namespace {

// A soft radial burst of rays — the shared "arrival" language.
void drawBurst(int cx, int cy, float t, Color c, int rays, float reach) {
    const float grow = 1.0f - t;  // rays extend as the beat lands
    for (int i = 0; i < rays; ++i) {
        const float a = static_cast<float>(i) * 6.2831853f / static_cast<float>(rays);
        const float r0 = 14.0f + 6.0f * grow;
        const float r1 = r0 + reach * grow;
        DrawLineEx({cx + std::cos(a) * r0, cy + std::sin(a) * r0},
                   {cx + std::cos(a) * r1, cy + std::sin(a) * r1}, 2.0f,
                   Fade(c, 0.65f * t + 0.15f));
    }
}

}  // namespace

std::optional<SummonKind> summonKindFor(const std::string& skillId) {
    if (skillId == "summon_goose") {
        return SummonKind::Goose;
    }
    if (skillId == "summon_sentinel") {
        return SummonKind::Sentinel;
    }
    if (skillId == "summon_spring") {
        return SummonKind::Spring;
    }
    return std::nullopt;
}

void drawSummonApparition(ResourceManager& resources, SummonKind kind, int centerX,
                          int centerY, float t, bool flashEnabled) {
    const ui::style::Palette& p = ui::style::palette();
    const float cx = static_cast<float>(centerX);
    const float cy = static_cast<float>(centerY);
    switch (kind) {
        case SummonKind::Goose: {
            // The realm's first goose, writ LARGE: the real goose sprite at
            // triple scale, bobbing on the sanctioned motion clock.
            if (flashEnabled) {
                drawBurst(centerX, centerY, t, p.gold, 12, 46.0f);
            }
            const float bob = std::sin((1.0f - t) * 12.5664f) * 3.0f;
            if (resources.hasTexture("actor.goose.battle")) {
                const Texture2D& tex = resources.texture("actor.goose.battle");
                const float scale = 3.0f;
                DrawTextureEx(tex,
                              {cx - tex.width * scale / 2.0f,
                               cy - tex.height * scale / 2.0f + bob},
                              0.0f, scale, Fade(WHITE, 0.35f + 0.65f * t));
            } else {
                DrawCircleV({cx, cy + bob}, 26.0f, Fade(p.gold, 0.7f * t + 0.2f));
            }
            break;
        }
        case SummonKind::Sentinel: {
            // The Starfall Sentinel: streaks falling from a great height into
            // a bright star lattice at the center.
            const float grow = 1.0f - t;
            if (flashEnabled) {
                for (int i = 0; i < 5; ++i) {
                    const float ox = (static_cast<float>(i) - 2.0f) * 26.0f;
                    const float top = cy - 90.0f - 18.0f * static_cast<float>(i % 3);
                    const float tip = cy - 20.0f * grow - 8.0f * static_cast<float>(i % 2);
                    DrawLineEx({cx + ox, top + grow * 60.0f}, {cx + ox, tip}, 2.0f,
                               Fade(p.crystal, 0.6f * t + 0.2f));
                }
            }
            const float r = 16.0f + 18.0f * grow;
            for (int i = 0; i < 4; ++i) {
                const float a = static_cast<float>(i) * 1.5708f + grow * 0.7854f;
                DrawLineEx({cx - std::cos(a) * r, cy - std::sin(a) * r},
                           {cx + std::cos(a) * r, cy + std::sin(a) * r}, 3.0f,
                           Fade(p.crystal, 0.75f * t + 0.25f));
            }
            DrawCircleV({cx, cy}, 6.0f + 4.0f * t, Fade(WHITE, 0.8f * t + 0.2f));
            break;
        }
        case SummonKind::Spring: {
            // The Radiant Spring: concentric ripples rising through a bloom.
            const float grow = 1.0f - t;
            for (int i = 0; i < 3; ++i) {
                const float r = (18.0f + 24.0f * static_cast<float>(i)) * (0.4f + 0.6f * grow);
                DrawCircleLinesV({cx, cy}, r, Fade(p.success, (0.7f - 0.18f * i) * t + 0.1f));
            }
            if (flashEnabled) {
                drawBurst(centerX, centerY, t, p.success, 8, 30.0f);
            }
            DrawCircleV({cx, cy - 10.0f * grow}, 8.0f, Fade(p.success, 0.7f * t + 0.25f));
            break;
        }
    }
}

}  // namespace cd::render
