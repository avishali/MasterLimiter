# Cursor handoff — AAX-signed DEV-enabled beta → testers

**Repo:** `MasterLimiter`, branch `main` at **`f82488d`** (UI/preset work committed). Builds against
the local sibling SDK `melechdsp-hq` (has F-3d Part A; SDK itself is NOT pushed — Quell-gated, fine
for a local build). You shipped `0.3.1-beta build 3` (`cd22ed8`) earlier; use that same established
release process. This prompt is the REQUIREMENTS + gotchas, not new mechanics.

## Goal
Ship **`0.3.1-beta build 4`** (DEV-enabled) to testers, AAX included and **PACE-signed so it opens in
Pro Tools**. Build 4 = build 3 PLUS, since then:
- crossover crash fixes (the saga was a stale binary — all genuinely fixed + live-confirmed now);
- **Default preset** = avishali's voicing (clipper OFF, xover 100/120/60, Lookahead release, color 0)
  baked into the fresh-instance defaults (`9000f54`); **factory presets pruned** to a single "Default";
- **compact default UI** (opens at 958×505) + relocated LUFS / Reset Peaks (`7b3248c`, `f82488d`).
Plus the whole crossover-fix series + F-3d clipper 8× (all on `main`).

## Hard requirements (get these right)

1. **`PLUGIN_DEV_MODE=ON`.** Testers must be able to touch the DEV parameters (crossover
   cutoff/transition/atten, lookahead, etc.) — that's the point of this phase. ⚠️ `build_release.sh`
   and `build_release_pkg.sh` force `-DPLUGIN_DEV_MODE=OFF`. Override to ON for this beta (same as
   the prior "embed DEV + dock panel" beta). Verify the DEV dock panel is present in the shipped build.

2. **AAX must be PACE wraptool-signed.** Unsigned AAX won't load in Pro Tools. Use the existing
   `scripts/.aax_wraptool.env` (iLok creds) + `scripts/wraptool_sign_aax.sh` (or `release_macos.sh`
   step 2). ⚠️ **WCGUID≠Product-GUID gotcha** (per project memory `release-signing-aax`) — the
   wrap-config GUID is not the product GUID; use the right one. After signing, VERIFY:
   `wraptool verify` (or `codesign -dvv`) confirms a valid PACE signature on the `.aaxplugin`.

3. **Bump the beta build number to `build 4`** (next after `build 3`).

4. **Apple sign + notarize + installer** (the rest of `release_macos.sh`: `sign_and_notarize.sh`
   + `create_installer.sh`/`build_release_pkg.sh`), then push to testers via the usual channel.

## Anti-stale verification (today's hard lesson — do NOT skip)
We just burned hours because Ableton loaded a stale 3 AM binary. Before shipping, PROVE the
artifacts are the current code:
- The main header shows the git hash — confirm the shipped build shows **`main@ed320d4`** (or the
  exact HEAD you build). The `[hash]` is the anti-stale token.
- `dwarfdump --uuid` / mtime on the AAX, AU, VST3 inside the installer match the freshly built
  artefacts. Don't ship if the hash/UUID doesn't match the build.

## Report back
- Confirm: DEV mode ON (dock panel present), AAX PACE-signed + verified (opens in PT — or note
  that PT validation is avishali's to do), notarization stapled, installer built, beta build number.
- The shipped git hash (must match `main` HEAD) + the AAX UUID.
- Where the installer/beta was pushed for testers.
- Commit any version bump: `release: 0.3.1-beta build N (DEV-enabled, AAX PACE-signed)`.
- Do NOT push the SDK (Quell-gated). Plugin `main` is already pushed.

## avishali validates
Load the signed AAX in Pro Tools, exercise it (incl. the DEV crossover params — they should
drag/commit cleanly with the duck-swap, no crash, since this build has 608b447). If PT-clean →
beta goes to testers.
