# SLICE ASCII-GATE — enforce ASCII UI strings at BUILD time (+ pre-commit), shared across the plugin line

**Status:** ready for Cursor · **Architect:** Claude · **Verify:** Claude (build fails on injected non-ASCII) · **Audition:** n/a
**Repos:** SDK `melechdsp-hq` (shared gate) + plugin `MasterLimiter` (adopt it). Additive; no product code change.
**Why:** the non-ASCII UI glyph bug "keeps coming back across the plugin line." Root cause (verified): `scripts/check_ui_ascii.sh` **works and exits 1 on a real violation**, but it is **not wired into the build or commit** — so nothing runs it; bad glyphs (em-dash, arrows) reach tester builds until someone checks by hand. Fix = make it run automatically, everywhere.

## The guarantee — CMake BUILD GATE (primary; do this)
Wire the ASCII check so **the build FAILS if any UI source has a raw non-ASCII string literal.** Every build (Cursor, avishali, CI) is gated — this is the actual guarantee (a pre-commit hook can be skipped; a build gate can't).
- Add a build step that runs the check over the plugin's UI sources (`Source/ui`) and **fails the build on non-zero exit**. Attach it so it runs BEFORE the plugin compiles/links (e.g. `add_custom_command(TARGET <shared-code target> PRE_BUILD COMMAND ... || fail)` or a custom target the plugin `add_dependencies` on). Use the check's exit code — it already returns 1 on violation.
- Keep it fast (the check is ~instant).

## Shared across the line — put the gate in the SDK (the "stop it everywhere" part)
Since every product has this problem, make the check + the CMake wiring **shared, adopted with one line per product**:
1. **Canonical check in the SDK:** move/copy `check_ui_ascii.sh` to `melechdsp-hq/tools/check_ui_ascii.sh` (single source of truth). (MasterLimiter's local copy can call the SDK one or be replaced.)
2. **CMake helper in the SDK:** a function, e.g. `mdsp_add_ui_ascii_gate(<target> <ui_dir>)` in `melechdsp-hq/cmake/MdspUiAsciiGate.cmake`, that adds the PRE_BUILD gate. Products include the SDK cmake dir and call it once.
3. **MasterLimiter adopts it:** `mdsp_add_ui_ascii_gate(MasterLimiter Source/ui)` (or equivalent) in its CMakeLists.
4. **Adoption note:** a short `melechdsp-hq/docs/UI_ASCII_GATE.md` (or a section in ENGINEERING_PLAYBOOK) telling other products how to adopt it (one include + one call + install the hook). This is what ends the recurrence family-wide.

## Early feedback — pre-commit hook (secondary; nice-to-have)
- Add a versioned hook `scripts/hooks/pre-commit` that runs the check and blocks the commit on violation, plus `scripts/install_hooks.sh` that wires it (`git config core.hooksPath scripts/hooks`, or copies it). Document it in the repo README/CONTRIBUTING. (Hooks aren't auto-installed per clone — the build gate is the real guarantee; this just catches it sooner.)

## Notes
- **Cross-platform:** the check is bash/perl (fine on macOS, the current build target). If/when Windows builds matter, port the scanner to a pure-CMake `-P` script (no bash/perl) — note as a TODO, don't block on it now.
- Don't change any UI strings here (UI-3 fixes the current violations); this slice only adds the automated gate.

## Build / verify
- Build clean with current (ASCII-clean, post-UI-3) sources — gate passes, build succeeds.
- **Negative test (the important one):** inject a non-ASCII char into a UI string, build → **build FAILS** with the check's message; remove it → build succeeds. (Claude will run this: temp non-ASCII string → build must fail.)
- Confirm the shared SDK helper is additive (no other product broken); MasterLimiter uses it.

## Output requirements
1. Retrieval log (CMake structure + where to attach the gate). 2. Diffs (SDK: check + cmake helper + doc; plugin: adopt + hook + installer). 3. Build output showing the gate ran. 4. Negative-test result (build fails on injected non-ASCII). 5. Confirm SDK change is additive. 6. Open questions.
