# M86 — CrystalForge catch-up & version 0.6.0

Authorized 2026-08-05 as the closing milestone of the M75–M86 expansion
program (see the program section in `docs/milestones.md`). Implemented
2026-08-07 on the post-M85 checkout (`f562d5c`).

## A. Status

**☑ complete (approved 2026-08-16)** — implemented 2026-08-07; owner
batch-approved M86–M97 on 2026-08-16. Evidence in §F.

## B. Goal (owner brief)

CrystalForge learns everything the program added, and the game's version
is deliberately renumbered to **0.6.0** (owner decision 2026-08-05: the
0.9.0 label overstated completeness).

## C. What was built

### The re-audit came first (the note's own instruction)

- **Descriptor catch-up: nothing owed.** Every field the program added
  (`mpDamagePct`, `initialStatuses`, `triggers` including the M85
  clone/drain keys, `statusImmunities`, the sleep-AI flags,
  `immuneToStatScale`, `guildTown`, `maxHeld`, `notSoldInTown`,
  `curesCurse`, `useLine`, `resistElements`/`resistPct`, `iconCategory`)
  shipped its descriptor **with its milestone**, enforced throughout by
  the M59 completeness sweep — as M75 §E and M84 §E predicted.
- **Enum-id flow verified, not assumed**: the pickers read the live
  `content::*Ids()` tables (one table, one truth); the `[editor]`
  enum-lists tests already pinned the M75 additions.
- The plan's "README claim fix" turned out to target the **CMake
  comment**: the README states the version once — by deferring to
  `CMakeLists.txt` — and carries no claim of its own.
- The ledger's execution-order note (M23 → M24 after M86, re-audited)
  was already in place from the program authorization.

### Two new editor categories

- `Category::EventFlavor` (`event_flavor.json`, M80) and
  `Category::CurioLore` (`curio_lore.json`, M85) on the standard
  machinery: browse/edit/save/validate through the real
  `parseEventFlavor`/`parseCurioLore` (the latter gained its header
  declaration — the M59 "declare what loadAll already calls" precedent),
  descriptors id/title/body and id/body (all loader-required), the
  one-line `InlineEntities` canonical style. The shipped files were
  **already byte-canonical**, so the one-time normalization was a no-op:
  `CrystalForge --canonicalize` reports **0 file(s) rewritten, 0
  content error(s)** over the full 12-file tree.
- In the game's loader both files remain OPTIONAL with their graceful
  fallbacks (M80 terms); only the editor expects its category files
  present, and a missing one degrades to a per-file load error like any
  other, never taking the tool down.
- The event-id known-ness rule still lives in the loader, so a typo'd
  event id fails validation on save instead of silently authoring
  nothing; curio-id known-ness stays a `[dragon]`-battery concern (the
  curio table lives a layer above content — the M85 reasoning).

### Latent editor defects found by the re-audit (catch-up mandate)

1. **`kCategoryCount` was still 9** after M63 grew the enum to 10 — the
   sidebar's Sim Lab / Test Runner indices sat one row too high, so the
   **Story row opened the Sim Lab and Story was unreachable** from
   M63 until now. The constant is 12, and `[editor]` now pins
   `categories().size() == kCategoryCount` plus enum-order agreement,
   making this drift class a suite failure.
2. **The dirty-star sidebar refresh truncated the sidebar**: it rebuilt
   the menu from `categories()` without re-appending the two M60
   surface rows.
3. **The story descriptor still bounded town at 1..9** after M85
   widened the loader to 1..10 (the Pale Jester's beat could not be
   authored); the label now reads "8 castle, 9 goose, 10 pale jester".
4. **`skeletonFor` injected a placeholder `"name"`** into every new
   entity, including categories whose schema has no name key (story,
   and now the two new files) — saving a stray unrecognized key. New
   entities only get the placeholder where a name descriptor exists.

### Version 0.6.0

- `project(CrystalDungeons VERSION 0.6.0)`; the comment rewritten to
  record the renumber decision ("the label should trail the game") and
  that 1.0.0 still waits on the M23 playtests. `Version.hpp` and the
  Windows `.rc` regenerate from it at configure time; the regenerated
  header was verified to read 0.6.0. Nothing else in the tree hardcodes
  a version string (audited: tests, packaging, README, docs — the
  ledger/roadmap mentions are historical records and stand).

## D. Schema, save & version implications

- No game-behavior change. The product version is 0.6.0; save (v1),
  score, content (v1), battle-rules (v15) and generation (v15) versions
  are independent and unchanged.
- One additive header declaration (`parseCurioLore`) — no signature or
  behavior changes in the loader.

## E. Deviations & decisions

1. The four editor fixes above were not in the planned scope text but
   sit squarely in the milestone's catch-up mandate ("CrystalForge
   learns everything the program added" + the final consistency sweep);
   each is small, behavior-restoring, and test-pinned. Recorded here
   rather than escalated — no player-facing rule, schema, or
   architecture was touched.
2. The two new categories keep their **id fields as plain strings**
   (not enum pickers over the known id tables): the event table's
   known-ness already fails validation through the real loader, and the
   curio table deliberately lives above the content layer. Conservative,
   reversible.
3. Curio-lore entries show no suffix column in the entry list (they
   have neither name nor title; the ids are descriptive). Event-flavor
   entries suffix with their title, like story beats.

## F. Automated validation (evidence)

- Debug build: **passed** (clean reconfigure; Version.hpp/.rc
  regenerated at 0.6.0; game + tests + CrystalForge all relinked).
- `[editor]` battery: **22 cases, 4,871 assertions, all passed** — now
  sweeping all 12 categories for descriptor coverage, value
  preservation, idempotence and byte-stability, plus the new pins:
  category-count/enum-order agreement, the M86 category shapes, the
  skeleton name rule, and loader-count equivalence for the two new
  files.
- `CrystalForge --canonicalize`: **0 file(s) rewritten, 0 content
  error(s) before and after** (the round-trip proof over the full
  post-M85 data tree); the working tree was verified untouched.
- Full Debug suite: **725/725 passed** (+3 new [editor] cases over
  M85's 722). Release build: **passed**; full Release suite: **721/721
  passed**. Capture sweep: **97/97 scenes clean** (the set is unchanged
  — M86 adds no game UI).

## G. Owner manual validation

1. Open CrystalForge: the sidebar lists twelve categories, then Sim Lab
   and Test Runner — confirm **Story now opens Story** and the two
   bottom rows open the two tools (the M63 regression).
2. Edit one event-flavor body and one curio-lore body, save, and see
   each in-game (an event panel in a dungeon; the curio's lore panel on
   the Maps screen). Confirm `git diff` shows only your text.
3. Edit the Pale Jester's story beat (town 10) — it is now reachable
   and its town field steps to 10.
4. Run the Sim Lab and the Test Runner once each — both surfaces should
   behave exactly as before.
5. Confirm the version: the game window title / packaged artifacts
   report **0.6.0**.

## H. Known limitations

- The editor requires its category files to exist to edit them; a data
  folder without the two optional files shows per-file load errors in
  the editor (the game itself is unaffected — they stay optional).
- New event-flavor entries start with an empty id and must be renamed
  to one of the known event ids before validation passes (the loader's
  known-ness rule; deliberate, §E.2).

## I. Final status

`complete (approved 2026-08-16)` — the ledger is authoritative; this line
lagged at the approval flip and was corrected 2026-09-02.
