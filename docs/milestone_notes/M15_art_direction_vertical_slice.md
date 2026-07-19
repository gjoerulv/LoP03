# M15 — Art-Direction Vertical Slice

## A. Status and authority

- **Status:** implemented, awaiting manual approval — the approval IS the
  art-direction gate (see §N). **Owner decision
  (2026-07-19): slice art/audio is generated in-project by Claude** (scripted
  pixel art + synthesized chiptune through the manifest); the art bible and
  slice double as a commissioning spec if pro art is wanted later. The
  direction gate at the end of M15 remains the owner's.
- **Last reviewed repository commit:** `67689b4`+M14 (2026-07-19).
- **Relationship to `docs/milestones.md`:** single authoritative detailed scope
  for the M15 ledger entry; the ledger holds status. On conflict, follow the
  authority order in `CLAUDE.md`.
- **Planning fidelity:** architectural/product. The slice contents below are
  stable, but the art bible's actual decisions (grid, palette, silhouette
  scale) are made *during* this milestone with the owner — they must not be
  presupposed. Re-audit and refresh this note against the then-current
  repository (especially M12 layout reality and the M14 manifest) before
  implementation begins.

## B. Problem statement

Everything on screen is a placeholder: code-generated tiles, rectangles, and
glyphs, with synthesized audio. Producing the full asset set without first
proving one coherent, affordable, readable direction risks the largest rework
cost in the whole program (roadmap failure mode: "final art before stable
contracts"). No art bible exists; pixel grid, silhouette scale, outline and
shading rules, and UI ornament language are all undecided.

## C. Goals

- An owner-approved **art bible** covering: mood and visual pillars; palette
  strategy and crystal accent language; authoritative pixel grid and tile
  size; character/enemy silhouette scale; outline and shading rules; animation
  frame conventions; UI ornament and icon rules; environmental composition;
  readable interaction hierarchy; prohibited imitation targets.
- One coherent final-quality vertical slice, delivered entirely through the
  M14 manifest:
  - title screen;
  - one town area and one service interior;
  - one compact Ruined Keep room (at the M16 candidate room scale);
  - one normal battle;
  - one boss battle;
  - one result screen;
  - representative music and SFX for those scenes.
- A measured production-cost estimate (time per tile/sprite/track) grounding
  the M17/M21 plans.

## D. Non-goals

- No full asset production (M17/M21) — the gate exists precisely to stop that.
- No gameplay, generation, or room-system changes (the compact room is a
  hand-placed preview, not the M16 system).
- No copyrighted or near-replica assets, layouts, or music — original or
  correctly licensed only.
- No resolution change; the slice must prove readability at 426×240 (or
  produce the evidence for a separate owner decision).

## E. Dependencies

- M12 layout system (UI framing that supports text must exist first).
- M14 manifest (all slice assets flow through it — this is also the manifest's
  real-world trial).
- Coordination with M16: the compact-room preview should use the candidate
  dimension ranges (common chambers ≈ 9–15 × 7–11 tiles) so the direction is
  proven at the intended scale.
- **Owner participation is intrinsic**: direction proposals, iteration, and
  the final approval gate.

## F. Proposed slices (provisional — refine before starting)

1. **Art bible draft** — written proposal with reference mockups; owner
   review/iteration; approval before asset work.
2. **UI + title treatment** — fonts, frames, cursor, title screen through the
   manifest.
3. **Town + service slice** — one town area, one interior, actor sprite
   direction.
4. **Dungeon room + battle slice** — compact Ruined Keep preview room; normal
   battle scene (backgrounds, enemy/party battle sprites); boss battle;
   result screen.
5. **Audio direction sample** — representative town/battle music and core UI/
   combat SFX via the manifest; sets the M21 bar.
6. **Readability and scaling review** — native 426×240 and supported window
   scales; grayscale check; owner gate decision.

## G. Expected systems affected

- `assets/` — first real content (textures/fonts/audio + manifest entries +
  credits/licensing records).
- Minimal C++ churn expected if M14 did its job; small state changes only
  where a slice screen needs a new logical ID.
- `docs/` — new art bible document (e.g. `docs/art_bible.md`).

## H. Data, schema, and save compatibility

- Gameplay data, saves, settings, seeds, scoring: **no impact.**
- Asset manifest: exercised, not redesigned; schema changes discovered here
  feed a reviewed manifest revision (owner approval) rather than ad-hoc edits.

## I. Automated validation

- Manifest/lint validation over all slice assets (IDs resolve, files exist,
  metadata valid, licenses recorded).
- Full suite green (no logic changes expected).

## J. Manual validation for the owner

- Review the art bible; approve or iterate.
- Play the vertical-slice path at native resolution, default window, and
  1920×1080; judge readability, mood, and coherence.
- Grayscale screenshot review: silhouettes, interactables, and danger cues
  must survive without color.
- Listen to the sample audio in context.
- Explicit gate decision: approve direction, request iteration, or reject.

## K. Acceptance criteria

- The owner approves the art bible and the coherent slice.
- Slice assets are readable at native resolution and supported scaling modes.
- All assets are original or correctly licensed, with provenance recorded.
- The production-cost estimate exists and is plausible against M17/M21 scope.
- Full asset production has **not** started before the gate decision.

## L. Risks and failure modes

- **Production creep** — "just one more room/enemy" turns the slice into
  M17; the slice list above is the boundary.
- **Direction churn** — repeated full-slice re-dos are expensive; iterate on
  the bible and mockups before producing polished assets.
- **Unaffordable direction** — a beautiful slice whose per-asset cost cannot
  scale to M17/M21; the cost estimate is a required deliverable, not
  optional.
- **426×240 conflict** — the chosen style may demand more pixels; escalate
  with evidence to the owner rather than deciding inside the milestone.
- **Licensing gaps** — any external asset without provenance blocks release
  later; record at acquisition time.
- **Manifest inadequacy discovered late** — schema gaps found here must go
  through a reviewed manifest revision, not hacks.

## M. Required documentation updates on completion

- `docs/milestones.md`; this note (as-implemented + deviations).
- New `docs/art_bible.md` (authoritative direction).
- `docs/asset_pipeline.md` — production workflow learnings.
- `docs/ui_style_guide.md` — final visual language for UI roles.
- `docs/completion_roadmap.md` — only if the gate decision changes M16–M21
  assumptions.
- Completion report per `docs/milestone_completion_template.md`.

## N. As-implemented record (2026-07-19)

Owner sourcing decision executed: all slice assets generated in-project by
deterministic scripts (`tools/asset_gen/generate_textures.ps1`,
`generate_audio.ps1`) — 25 PNGs + 3 music WAVs, all original, recorded in
`assets/credits.md`, validated by the manifest test. 181/181 tests pass.

**Delivered:** `docs/art_bible.md` (proposal pending this gate); title
emblem + framed title; textured town (7 tiles) + hooded player sprite;
Ruined Keep tile set; chest/gate/boss marker sprites; six class battle
sprites + normal/elite/boss enemy sprites; nine-patch UI frame on the
battle/pause/Inn/result panels; town/dungeon/battle chiptune loops replacing
the synthesized hums. Render integration keys off the catalog per element
with the colored-shape path kept as fallback (`Dungeon.themeId` added so
theme tiles resolve; sprite lookup tries specific ids first so M17 per-enemy
art is drop-in). Captures: `docs/screenshots/m15_slice/` (title, town,
Guild, keep room ×2, framed pause).

**Deviations / gate notes:**
1. Battle screens could not be reached by scripted capture — class/enemy/
   boss sprites and the framed battle panel follow the same verified pattern
   but need the owner's eyes (matrix rows 24–31).
2. The "one service interior" is represented by the framed Inn panel; no
   interior scene art exists (services are panel screens by design).
3. Font unchanged (raylib default) — a pixel font is a separate owner
   decision, deliberately not smuggled in.
4. Production-cost evidence: each additional tile/sprite is a small
   generator function (minutes); the honest ceiling is deliberate simple
   pixel art. If the owner wants a higher ceiling for M17, the bible + this
   slice are the commissioning spec.

**The M15 gate is the owner's:** approve this direction for M17/M21
production, request iteration (palette/shapes/music are one-script changes),
or reject in favor of commissioned/licensed art.
