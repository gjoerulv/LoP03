#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <vector>

#include "content/Enums.hpp"
#include "states/ElementChip.hpp"  // M53: ui::elementAccent
#include "ui/UiDraw.hpp"
#include "ui/UiStyle.hpp"

// M46 procedural UI kit: pure layout math plus contrast guards for both
// palettes. Drawing itself needs a window and is validated by the capture
// tool; everything here runs headlessly.

namespace ui = cd::ui;
namespace style = cd::ui::style;

namespace {

// WCAG-style relative luminance / contrast ratio (engineering guard, not a
// formal conformance claim — mirrors docs/ui_style_guide.md §4 targets).
double channel(unsigned char v) {
    const double u = static_cast<double>(v) / 255.0;
    return u <= 0.03928 ? u / 12.92 : std::pow((u + 0.055) / 1.055, 2.4);
}

double luminance(Color c) {
    return 0.2126 * channel(c.r) + 0.7152 * channel(c.g) + 0.0722 * channel(c.b);
}

double contrast(Color a, Color b) {
    double la = luminance(a);
    double lb = luminance(b);
    if (la < lb) {
        std::swap(la, lb);
    }
    return (la + 0.05) / (lb + 0.05);
}

}  // namespace

TEST_CASE("meterFillWidth: empty, sliver, monotonic, full", "[ui-kit]") {
    CHECK(ui::meterFillWidth(0, 100, 40) == 0);
    CHECK(ui::meterFillWidth(-5, 100, 40) == 0);
    CHECK(ui::meterFillWidth(50, 0, 40) == 0);   // malformed max: draw nothing
    CHECK(ui::meterFillWidth(1, 1000, 40) == 1); // a sliver of life stays visible
    CHECK(ui::meterFillWidth(100, 100, 40) == 40);
    CHECK(ui::meterFillWidth(150, 100, 40) == 40);  // overheal clamps
    int previous = 0;
    for (int hp = 0; hp <= 200; ++hp) {
        const int w = ui::meterFillWidth(hp, 200, 38);
        CHECK(w >= previous);
        CHECK(w <= 38);
        previous = w;
    }
}

TEST_CASE("planHintGap: wide, narrow, overflow fallback", "[ui-kit]") {
    CHECK(ui::planHintGap({}, 12, 6, 100) == 12);
    CHECK(ui::planHintGap({40, 40, 40}, 12, 6, 400) == 12);   // 144 <= 400
    CHECK(ui::planHintGap({100, 100, 100}, 12, 6, 320) == 6); // 324 > 320, 312 <= 320
    CHECK(ui::planHintGap({150, 150, 150}, 12, 6, 400) == -1);
    CHECK(ui::planHintGap({120}, 12, 6, 120) == 12);  // single group, no gaps
}

TEST_CASE("lighten clamps and preserves alpha", "[ui-kit]") {
    const Color c = ui::lighten(Color{250, 10, 128, 77}, 40);
    CHECK(static_cast<int>(c.r) == 255);
    CHECK(static_cast<int>(c.g) == 50);
    CHECK(static_cast<int>(c.b) == 168);
    CHECK(static_cast<int>(c.a) == 77);
    const Color d = ui::lighten(Color{10, 10, 10, 255}, -40);
    CHECK(static_cast<int>(d.r) == 0);
}

TEST_CASE("both palettes hold the documented contrast floors", "[ui-kit]") {
    for (const bool high : {false, true}) {
        style::setHighContrast(high);
        const style::Palette& p = style::palette();
        INFO((high ? "high-contrast" : "standard") << " palette");
        // Normal text >= 4.5:1 on every surface it renders over.
        CHECK(contrast(p.text, p.panel) >= 4.5);
        CHECK(contrast(p.text, p.panelRaised) >= 4.5);
        CHECK(contrast(p.text, p.panelInset) >= 4.5);
        CHECK(contrast(p.text, p.rowFill) >= 4.5);
        CHECK(contrast(p.textDim, p.panel) >= 4.5);
        CHECK(contrast(p.dangerText, p.panel) >= 4.5);
        // Hints, disabled, accents, meters: >= 3:1 (secondary/non-text UI).
        CHECK(contrast(p.textHint, p.footerFill) >= 3.0);
        CHECK(contrast(p.textHint, p.panel) >= 3.0);
        CHECK(contrast(p.disabled, p.panel) >= 3.0);
        CHECK(contrast(p.cursor, p.rowFill) >= 3.0);
        CHECK(contrast(p.gold, p.keycapFill) >= 3.0);
        CHECK(contrast(p.gold, p.chipFill) >= 3.0);
        CHECK(contrast(p.success, p.panel) >= 3.0);
        CHECK(contrast(p.hpFill, p.meterBack) >= 3.0);
        CHECK(contrast(p.mpFill, p.meterBack) >= 3.0);
        CHECK(contrast(p.crystal, p.panel) >= 3.0);
        CHECK(contrast(p.magic, p.panel) >= 3.0);
        // The top-left-light construction needs a strict border ordering.
        CHECK(luminance(p.borderLight) > luminance(p.borderMid));
        CHECK(luminance(p.borderMid) > luminance(p.borderDark));
        CHECK(luminance(p.borderDark) > luminance(p.ink));
        // Selection slab must stand out from the panel it sits on.
        CHECK(contrast(p.rowFill, p.panel) >= 1.2);
    }
    style::setHighContrast(false);
}

TEST_CASE("high-contrast keeps every new role at least as bright", "[ui-kit]") {
    style::setHighContrast(false);
    const style::Palette standard = style::palette();
    style::setHighContrast(true);
    const style::Palette& high = style::palette();
    // Text-bearing and accent roles brighten (or hold) in high contrast.
    CHECK(luminance(high.text) >= luminance(standard.text));
    CHECK(luminance(high.textDim) >= luminance(standard.textDim));
    CHECK(luminance(high.textHint) >= luminance(standard.textHint));
    CHECK(luminance(high.borderLight) >= luminance(standard.borderLight));
    CHECK(luminance(high.borderMid) >= luminance(standard.borderMid));
    CHECK(luminance(high.crystal) >= luminance(standard.crystal));
    CHECK(luminance(high.gold) >= luminance(standard.gold));
    CHECK(luminance(high.hpFill) >= luminance(standard.hpFill));
    CHECK(luminance(high.mpFill) >= luminance(standard.mpFill));
    // Fills darken (or hold) so separation increases, never decreases.
    CHECK(luminance(high.canvas) <= luminance(standard.canvas));
    CHECK(luminance(high.panel) <= luminance(standard.panel));
    CHECK(luminance(high.footerFill) <= luminance(standard.footerFill));
    style::setHighContrast(false);
}

TEST_CASE("elementAccent: every element maps to an existing palette role", "[ui-kit]") {
    // M53: the equip-shop weapon-element chip reuses M46 palette roles rather
    // than introducing new colours. Each element resolves to one of them, and
    // the mapping is stable and readable (chip labels are large text vs the
    // canvas — a 3:1 target).
    namespace content = cd::content;
    const style::Palette& p = style::palette();
    auto sameColor = [](Color a, Color b) {
        return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
    };
    CHECK(sameColor(ui::elementAccent(content::Element::Fire, p), p.danger));
    CHECK(sameColor(ui::elementAccent(content::Element::Ice, p), p.crystal));
    CHECK(sameColor(ui::elementAccent(content::Element::Lightning, p), p.gold));
    CHECK(sameColor(ui::elementAccent(content::Element::Earth, p), p.success));
    CHECK(sameColor(ui::elementAccent(content::Element::Holy, p), p.gold));
    CHECK(sameColor(ui::elementAccent(content::Element::Dark, p), p.magic));
    // None falls back to muted text, never an accent role.
    CHECK(sameColor(ui::elementAccent(content::Element::None, p), p.textDim));
    // Every accent reads against the canvas at the large-text bar.
    for (content::Element e : {content::Element::Fire, content::Element::Ice,
                               content::Element::Lightning, content::Element::Earth,
                               content::Element::Holy, content::Element::Dark}) {
        INFO(content::elementDisplayName(e));
        CHECK(contrast(ui::elementAccent(e, p), p.canvas) >= 3.0);
    }
}
