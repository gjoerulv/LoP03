# Crystal Dungeons — Art Bible

> Status: **proposal pending the M15 owner gate.** Sourcing (owner decision
> 2026-07-19): assets are generated in-project by scripted pixel art +
> synthesized chiptune (`tools/asset_gen/`), fully original; this document
> doubles as a commissioning spec if professional art is wanted later.
> Nothing here is final until the owner approves the vertical slice.

## 1. Mood and visual pillars

1. **Readable first.** Every gameplay-relevant thing (exits, enemies, chests,
   the player) must read in silhouette at native 426×240. Atmosphere never
   outranks legibility.
2. **Humorous 8-bit-plus fantasy micro-caricature** (owner-approved, M46 —
   supersedes the original "restrained dark fantasy / quiet dread" wording):
   Atari-era geometric economy, early-console silhouette readability, a
   brighter storybook palette, deliberately oversized equipment. Playful but
   not childish; humor comes from small visual exaggerations, never at the
   cost of information. The world/tile/sprite art authored under the earlier
   mood remains shipped and compatible — the M46 UI palette warms the frame
   around it.
3. **Crystal as light.** Luminous cyan/violet crystal accents are the world's
   focal contrast — used sparingly (markers, UI glints, the emblem, boss
   presence), never as wallpaper.
4. **Efficiency aesthetic.** Clean geometry and calm framing suit a game
   about doing things in the fewest turns; no visual busywork.

## 2. Palette strategy

Base ramps (hex; ±1 step variation allowed, no new hues without a bible
update):

- **Night base:** `#0E0C14` (outline/void), `#12101A`, `#1C1826`, `#262233`
- **Stone:** `#3A3646`, `#4A4658`, `#5C566B`, `#736D82`
- **Earth/wood:** `#4A3B2A`, `#6B5138`, `#8A6D48`, `#A98F63`
- **Vegetation:** `#22371E`, `#2A4432`, `#3A5C40`, `#4E7A50`
- **Water:** `#22304E`, `#2A3C64`, `#34506E`, `#4A6A8A`
- **Crystal accents:** cyan `#64E0DC`, violet `#9C6CE8` (+`#C9A8F5` glint)
- **Signal colors:** danger `#D85A5A`, gold/reward `#E8D670`,
  heal `#8CD98C`
- Class accents: Knight `#C0C6D0`, Ranger `#4E9A50`, Mage `#6C7CE8`,
  Cleric `#E8E2C8`, Rogue `#8A5FB0`, Guardian `#C87E3A`
- **Flesh/brute (M73):** `#2E1820`, `#43242B`, `#5C3038`, `#7A4650`
- **Boss void (M73):** `#1E1628`, `#2A2038`, `#3A2C4E`, `#4E3C68`
- **Neutral white (M73):** `#B8B8BC`, `#D8D8D4`, `#F2F2F0`

> The last three ramps were **completed, not invented, in M73** (pending the
> owner's sign-off on that milestone). Their middle steps have shipped since
> M26 (brute flesh, boss violet-black) and M62 (the geese whites) without ever
> being recorded here, and as two-step ramps they could not satisfy this
> section's own 3-band rule. M73 adds the missing darkest/highlight steps —
> six new hex values — and changes no previously shipped hue.

Rules: 3-band shading (shadow/base/highlight) from the ramps above; sparse
single-pixel speckle for texture, no heavy dithering; signal colors are
reserved for their meanings and never decorative.

## 3. Pixel grid and scale

- **Tile size: 16×16** (authoritative; matches `Tilemap::kTileSize`).
- Overworld actors/props: **12×12** inside the 16px tile (collision box
  unchanged — art never changes collision).
- Battle sprites: **24×24** (bosses **36×36** since M73; 32×32 before that),
  side-profile, party faces left, enemies face right.
  The boss canvas is a hard limit, not a preference: `BattleState::drawUnit`
  anchors bottom-centre at `sy = enemyBaseY() + 16 - tex.height`, and
  `enemyBaseY()` drops from 36 to 20 once a fight fields 5+ enemies (three
  authored boss teams do — Rush Tyrant, Abyssal Tyrant, the Deadly Duck). 36
  rows lands the sprite top at exactly y = 0; anything taller loses its crown
  off the top of the screen. 36 columns is likewise the widest that stays
  inside the 40px unit footprint. Growing either needs an `enemyBaseY()`
  change and owner approval.
- UI frame: 24×24 nine-patch with 8px borders; icons 8×8 or 12×12.
- Native-resolution authoring only; nearest-neighbor scaling; no mixed-scale
  pixels ("pixel-perfect or absent").

## 4. Outline and shading

- Full 1px outline in `#0E0C14` on every actor, prop, and marker (auto-traced
  by the generator); environment tiles have **no** outer outline (they tile).
- Light source: top-left, consistent everywhere.
- Interior detail minimal: one shadow band + one highlight; faces are
  abstract (hood/helm shadows, no facial features at this scale).

## 5. Silhouette identity

- **Player (overworld):** small hooded traveler, gold cloak accent — the
  only warm-gold moving thing in a scene.
- **Classes (battle):** one shared humanoid base, identity via silhouette
  add-ons + accent: Knight sword+shield, Ranger bow+hood, Mage staff+hat,
  Cleric robe+rod, Rogue dagger+scarf, Guardian tower shield.
- **Enemies:** hunched/bestial shapes vs. the party's upright ones; elites
  add horns + violet accent; bosses are larger with a crown/crest and violet
  crystal glow. Danger is shape+size+accent, never hue alone. Realised as
  **67 sprites — 27 normal + 25 elite at 24×24, 15 bosses at 36×36** —
  generated by `generate_textures.ps1`; a presentation-lint guard fails the
  suite on any enemy/boss id lacking its own sprite.
- **Enemy silhouette rules (M73).** The binding test is
  `tools/asset_gen/preview.ps1`'s silhouette sheet: if two enemies read as the
  same shape in solid black, they fail regardless of how they look in colour.
  1. **Silhouette first.** Angular, deliberate shapes with Atari-era geometric
     economy — flat tops, hard 45° diagonals, no organic blob rounding.
  2. **Negative space is the strongest tool.** A gap between weapon and body,
     holes between ribs, daylight between legs. Every sprite that failed
     review during M73 failed because its parts merged into one mass.
  3. **Oversized equipment is the humour lever.** The club, cleaver, staff,
     crown, tome, banner, drum and horn are comically large relative to the
     body and still inside the canvas.
  4. **One clear read per enemy.** A goblin grunt is obviously a goblin grunt
     at 24×24.
  5. **Robes are interchangeable, so headgear carries the caster silhouette** —
     cone hood, antlers, mitre, plague beak, halo ring.
  6. **Tier is posture and mass, never colour.** The generic normal/elite pair
     is the same beast on all fours and reared up, so the tier difference
     survives grayscale.
- **Interactables:** chest (gold trim), gate marker (crossed blades on red),
  boss marker (crowned skull on violet) — each unique in silhouette.

## 6. Environment composition

- **Town:** warm and safe — greens/earth, framed by a tree border; buildings
  read as solid roofed blocks with clear doors.
- **Ruined Keep** (slice theme): cracked slab floors, coursed masonry walls,
  broken-arch doors; sparse rubble speckle; cool stone ramp.
- Later themes (M17) differentiate by **shape language**, not palette swap:
  Crystal Mine = supports/rails/clusters; Hollow Forest = roots/organic
  boundaries. Grayscale composition must still distinguish them.
- Decorative density stays low near doors, markers, and paths.
- **Town service screens (M27):** each of the six services (Inn, Item Shop,
  Equip Shop, Training Hall, Scoreboard, Guild) has its own full-screen
  background — a distinct dark tinted gradient plus a low-contrast themed motif
  biased to the top/edges. Legibility of overlaid text is the binding
  constraint (§7): the base stays near the prior flat fill so M12 contrast
  holds, and a missing background falls back to that flat fill.

## 7. UI ornament (M46)

- The UI is a **procedural kit**, not framed art: `drawFrame` variants with
  stepped ink corners, top-left light, selection slabs, keycap footers,
  framed meters, and sparse crystal/gold pips — palette roles and
  construction rules live in `src/ui/UiStyle.hpp` / `src/ui/UiDraw.hpp`
  (see `docs/ui_style_guide.md` §4–§9). The M15 nine-patch frame texture
  remains loadable/manifest-replaceable but is unused by screens.
- The crystal emblem (32×32 cluster) marks the title screen only.
- Text uses the original pixel bitmap font delivered in **M25**; see
  `docs/ui_style_guide.md` §2.

## 8. Animation conventions (implemented, M17)

- Walk cycles 3 frames (stand / step A / step B) at ~6.7 fps (0.15s per
  frame); frame 0 doubles as the stand pose so a stopped actor is just the
  animation at t = 0. Sheet rows encode facing: down, up, left, right.
  Indicator pulses 2 frames at 0.4s. Effects ≤ 6 frames.
- Sprites draw centered on the collision-rect center (anchor rule changed
  from the M15 "bottom-center" plan: center-anchoring keeps 12×12 actors in
  16px tiles visually stable in top-down view and decouples art size from
  collision).

### 8b. Theme identity in production (M17)

Composition carries each theme; palettes stay within §2:

- **Ruined Keep** — coursed masonry, cracked slab floors, rubble-pile
  accents. Identity: broken geometry.
- **Crystal Mine** — dark rough rock, timber support beams and posts on
  every wall, timber-framed openings, cyan/violet crystal-cluster accents
  (the only luminous elements). Identity: braced excavation.
- **Hollow Forest** — mossy leaf-litter floors, massive trunk-and-root
  walls with moss caps, root archway openings, mossy shrine-stone accents
  with carved cyan sigils. Identity: overgrown enclosure.
- **Overworld enemy silhouettes** differentiate by shape, never color
  alone: plain hunched beast (normal), horned + war-banded (dangerous),
  tall crowned figure (boss-tier).

## 9. Audio direction

- Chiptune-style: square-wave lead, triangle bass, generous decay; calm
  major-mode town, sparse minor dungeon with drone, driving battle at
  ~140 BPM. Loops are seamless and short (6–10s for the slice); real
  arrangements in M21 follow this language.

## 10. Prohibitions

- No copyrighted or imitative material: no Final Fantasy or other JRPG
  sprite/UI/monogram look-alikes, no borrowed monster/spell designs, no
  sampled game audio. Everything original or properly licensed with a
  `credits.md` record.
- No signal-color reuse, no outline-less actors, no off-grid pixels, no
  decorative crystal spam.

## 11. Production notes

Generators live in `tools/asset_gen/` (PowerShell + System.Drawing; WAV
writer for audio) and are deterministic — rerunning them reproduces every
generated asset byte-for-byte from this bible's constants. Per-asset cost is
effectively zero once a shape function exists; the honest ceiling is
"deliberate simple pixel art", which the owner accepts or replaces at the
M15 gate.

**Authoring enemy and boss sprites (M73).** They are no longer drawn by
calling ellipse primitives blind. Each is an **explicit ASCII pixel grid** —
one block per sprite, one character per pixel, keyed to the §2 ramps — so a
row is directly readable and surgically editable in a diff. `Draw-Grid`
rejects ragged rows and unknown palette keys, reporting every fault at once;
`Save-EnemyGrid` rejects any boss that is not 36×36. The section calls **no
random helper**: every speckle pixel is hand-placed, so adding, removing or
reordering a sprite can never shift another file's bytes.

**Review before you believe it.** `tools/asset_gen/preview.ps1` composites the
sprites into a magnified labelled contact sheet, a **silhouette sheet** (alpha
as solid black on white — the binding artefact), and a 1× strip at the native
426-wide canvas. Run the generator, run the preview, then *actually open the
PNGs* before calling a sprite finished. Use `-Only a,b,c` with a higher
`-Zoom` to review one family at a time. Current sheets live in
`docs/sprite_review/`, alongside the pre-M73 `baseline_*` set for comparison.
