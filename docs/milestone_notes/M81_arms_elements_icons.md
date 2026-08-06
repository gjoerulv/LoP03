# M81 — Arms, elements & icons: resist accessories, elemental weapons, gear icons

Authorized 2026-08-05 as part of the M75–M86 expansion program (see the
program section in `docs/milestones.md`). Implemented 2026-08-06 on the
post-M80 checkout (`ccb4d2a`).

## A. Status

**◑ implemented, awaiting manual approval** — implemented 2026-08-06.
Evidence in §F.

## B. Goal (owner brief)

The party gets an elemental defense layer (per-element half-damage
accessories plus two new legendaries), more elemental weapons filling the
town tiers, and every piece of equipment gets a categorized icon wherever
it is listed.

## C. What was built

### Schema (the one new field)

- **`ItemDef::iconCategory`** — optional string, validated against the new
  `content::kIconCategoryIds` vocabulary (sword / axe / dagger / bow /
  staff / mace / spear / shield / armor / accessory / relic — matched to
  the real catalog), gear only. `iconCategoryFor(def)` resolves the
  authored value or the slot-derived default (Relic→relic, Armor→armor,
  Accessory→accessory); a **weapon must author one** (no slot default —
  a sword and a staff share a slot; the loader rejects the omission).
  `gearIconTextureId(def)` maps it to the manifest key
  `ui.icon.<category>` — the single convention render code and the lint
  share. CrystalForge descriptor shipped with the field (M78 precedent);
  `--canonicalize` settled the files and a second run rewrites zero.
- **Re-audit finding**: the note planned an `elementResist` map, but the
  M75 hook already shipped as `resistElements` (list) + `resistPct`
  (one percent for the list) with **best-piece-wins stacking**
  (`std::max` per element at `buildBattle`, never a sum) and its
  CrystalForge descriptors. No engine or schema work was needed for
  resists — M81 authored content onto the live hook.

### The ward set (six resist accessories)

All equipment/accessory, rarity rare, resist 50 %, **no stat bonus** (a
ward is a real trade against a stat accessory, not a strict upgrade),
priced at-or-under each town's stat accessory. Buyable + chest pool via
the normal catalog rules.

| id | element | town | price |
|---|---|---|---|
| `flameward_charm` | Fire | 2 | 340g |
| `stormward_charm` | Lightning | 2 | 340g |
| `frostward_charm` | Ice | 3 | 470g |
| `nightward_charm` | Dark | 3 | 470g |
| `stoneward_charm` | Earth | 4 | 600g |
| `lightward_charm` | Holy | 4 | 600g |

The equip-shop detail line, the equip-diff row and the Details overlay
all state the resist outright ("Resists Fire 50%" via the shared
`equip::resistSummary`), so the piece's identity is never hidden in a
description.

### Two legendaries

- **`overwound_pocketwatch`** — +200 SPD (the owner's number, verbatim),
  6000g. - **`motley_aegis`** — all six elements at −50 %, no stat bonus,
  7500g. Both rarity `legendary`, which IS pool membership: they join the
  shared black-market / boss-drop pool (`legendaryDropPool`) with no
  extra wiring, proven by test.

### Five elemental weapons (t2–t5 had none)

Stats on the M54 tier curve with a ~15 % price premium over the tier's
plain piece, so the budget option stays a real choice. Each also extends
a starved weapon family (bow and dagger had one entry each since t1).

| id | element | town | stats | price | icon |
|---|---|---|---|---|---|
| `winterbrand` | Ice | 2 | ATK 12 | 460g | sword |
| `stormstring` | Lightning | 3 | ATK 15, SPD +2 | 700g | bow |
| `quakemaul` | Earth | 4 | ATK 20, SPD −1 | 1080g | mace |
| `emberfangs` | Fire | 5 | ATK 22, SPD +4 | 1560g | dagger |
| `vigil_lance` | Holy | 5 | ATK 23 | 1560g | spear |

### Coverage audit (enemy-side, M48 sparse bar)

Found: **earth was dealt by nobody** (`stone_edge` existed in no kit),
fire by one normal (`dark_acolyte`), holy by one castle elite
(`royal_guard_sword`). Ice, lightning and dark healthy. Added, content
only, all with existing skills (skills.json untouched, pin stays 72):

- `stone_golem` (t1 normal, MAG 0→10): + `stone_edge` — and since the
  golem and the mud crawler are the **Obsidian Colossus's minions**, his
  fight becomes the earth fight with no boss edit.
- `titan_guard` (t7 elite, MAG 0→12): + `stone_edge` (kit-first).
- `soul_render` (t6 elite, MAG 24): + `inferno` kit-first — a party-wide
  fire nuke, the fight the Flameward earns its slot in.
- `standard_bearer` (t1 normal, MAG 10→12): + `smite` (a consecrated
  banner). MAG raises are cast-fuel only — magic defense reads DEF, not
  MAG, so nothing gets tankier. A test pins that every recipient's
  derived MP affords its new cast.

### Gear icons

- Eleven hand-authored **10×10** pixel grids in a new **RNG-free section
  at the very end** of `generate_textures.ps1` (`Save-IconGrid`, which
  hard-fails any non-10×10 grid), written to
  `assets/textures/ui/icons/<category>.png` and registered in the
  manifest as `ui.icon.<category>`. SHA-256 sweep proves all 196
  pre-existing PNGs byte-identical (the M73 guarantee holds).
- 10×10 is the size the UI actually renders: menu rows are 14px at font
  10, the party panel's gear lines sit on a 10px pitch — the largest
  square that fits every site at 1×.
- **Render sites**: `ui::MenuItem` gains an optional `icon` id and
  `drawMenuScrolled` a trailing `ResourceManager*` (default null; old
  call sites unchanged; once any row has an icon, all labels indent so
  the column stays straight). Wired in the equip-shop buy/equip/slot
  lists, the **Armory Ghost** trade list (in the spirit of "wherever
  equipment is listed", beyond the note's original three sites), the
  party panel's gear lines, and the black-market offer at 2×.
  `ui::drawGearIcon` draws nothing for a missing texture — placeholder
  discipline.
- Review harness: `tools/asset_gen/preview_icons.ps1` (new) composites
  the set at 12× on dark and light rows →
  `docs/sprite_review/icons_contact.png`. One redraw cycle already
  happened: the first axe read as a hammer (colliding with the mace) and
  was given a flat vertical cutting edge.

## D. Schema, save & version implications

Optional content field only (`iconCategory`); **no battle-rules bump**
(v15), **no generation bump** (v14), no save motion. The 13 new items
join chest / black-market / boss-drop pools by the existing catalog
rules: pool size does not change RNG consumption (one `rng.range` per
chest), so layouts for a given seed are untouched; *which* item a seed
yields shifts — the same class of change as every prior gear-adding
milestone (M43/M53/M76). Stored black-market offers in saves keep their
itemId.

## E. Deviations & conflicts (owner attention)

1. **The M48 "no weapon element is ever an immunity" rule was narrowed —
   this is the one real design conflict of the milestone.** The approved
   plan orders Ice/Lightning/Earth weapons; the shipped roster already
   carries ice, lightning and earth **immunities** (frost imp, rune
   sentry's kin, crystal guardian, stone golem, titan guard, frost
   monarch, obsidian colossus…). The plan outranks the design doc
   (document authority), so M81 ships the weapons and narrows the rule
   to its true core: the **intrinsic** fire/holy basics (the Dragon's
   bite — a skill-less class's only tool) stay immunity-free absolutely;
   a **wielded** element meeting an immunity is an informed trade (M53
   shop chip, bestiary row, "Immune" float), and a lint now demands
   every weapon element weak-hit at least one foe. `game_design.md` and
   `test_elements.cpp` updated accordingly. **Veto path**: reshuffling
   the three colliding weapons to fire/holy variants is a small content
   change — say the word. Residual edge worth knowing: a Dragon wielding
   `winterbrand` into an ice-immune fight deals 0 with basics (a real
   weapon element overrides the bite); the swap-back is a town visit.
2. The planned `elementResist` map shipped in M75 as
   `resistElements`+`resistPct` — recorded in §C, no work needed.
3. Armory Ghost added as a fourth icon site (it lists gear with the same
   idiom; leaving it bare would have been the only icon-less gear list).
4. Ward charms carry **no stat bonus** (the note left it open): pure
   resist identity keeps them trades, not upgrades.

## F. Automated validation (evidence)

- Debug build: **passed** (VS2022 dev shell, `cmake --build --preset
  debug`).
- `CrystalForge --canonicalize`: adopted canonical form for the edited
  JSONs; second run rewrites **0 files**, 0 content errors.
- Generator determinism: SHA-256 sweep over all generated PNGs — 196
  pre-existing byte-identical, exactly 11 new icon files.
- Debug suite: **688/688 passed** (VS2022 dev shell, `ctest --preset
  debug`, after the rebuild that picked up the narrowed elements lint —
  the pre-rebuild pass honestly failed exactly that one stale case).
- Release build + suite: **684/684 passed** (`msvc-release`/`release`
  presets; the smaller count is the usual Debug-only set).
- Capture sweep: **88/88 scenes clean**, zero overflow — including the
  new `88_ward_charms` (icons + the "Resists Earth 50%" detail line)
  and the icon-bearing scenes 09/24/30/63 (equip shop), 27 (black
  market, relic icon at 2×), 66 (Armory Ghost), 79 (party panel).

## G. Owner manual validation

1. **Icons**: open the sheet `docs/sprite_review/icons_contact.png`
   (dark + light rows), then in game: equip-shop buy lists (all three
   categories), a member's equip flow, the party panel, a black-market
   offer, an Armory Ghost. Judge readability at native 426×240 and in
   High Contrast.
2. **Wards**: buy a Flameward in town 2+, fight a fire caster (dark
   acolyte; the soul render at t6 for the real test) with and without it
   — the halved hit and the log's element line should make the charm
   feel true. Check the shop detail line says the resist.
3. **Weapons**: shop the five across t2–t5; check each town's plain
   piece still reads as the budget option; swing Winterbrand at a frost
   imp and confirm the "Immune" float reads as information, not a bug —
   **this is the M48-narrowing trade in play (§E.1); judge it**.
4. **Coverage**: meet a stone golem (earth), the soul render's inferno
   (t6), a standard bearer's smite — none should feel out of character.
5. **Legendaries**: token-buy or drop-hunt the Pocketwatch (+200 SPD:
   its bearer should simply always go first) and the Motley Aegis.
6. Confirm nothing else moved: consumable shop, battle layouts, save
   compatibility (an old save's stored market offer still resolves).

## H. Known limitations

- Icons are per-category, not per-item (the plan's shape); a legendary
  sword shares the sword glyph.
- The wards are mono-element and the Aegis is the only multi-resist
  piece; resist never exceeds 50 % anywhere (best-piece-wins).
- Fire coverage below t6 is still just the dark acolyte — deliberate
  (sparse means sparse); the Flameward's showcase is the t6+ soul
  render.
- CrystalForge cannot AUTHOR new icon categories (the vocabulary is
  code); it edits `iconCategory` per item via the shipped descriptor.

## I. Final status

`implemented, awaiting manual approval`
