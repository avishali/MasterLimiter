# Cursor Task — Slice 18 CLOSE (signing / notarization / AAX tooling)

Paste this entire file into Cursor as the task prompt. Do not edit it.

> **Product-only close** of the distribution tooling (macOS sign+notarize +
> conditional AAX build + PACE wraptool + docs + manual). No HQ → no submodule
> bump. Commit + push, FF `main`. **Never commit real credentials** — only the
> `.example` template (the real `scripts/.aax_wraptool.env` stays gitignored).

────────────────────────────────────────
0. SYSTEM RULES PREAMBLE
────────────────────────────────────────

- Stage by **explicit path only**. No force-push/amend/skip-hooks. Auth via
  system credential helper. `Co-Authored-By: Claude` trailer.
- No HQ commit / no submodule bump. Confirm `scripts/.aax_wraptool.env` and any
  real `.env` are NOT staged (gitignored); only `.aax_wraptool.env.example` is.

────────────────────────────────────────
1. PRE-FLIGHT
────────────────────────────────────────

```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter
git branch --show-current        # slice-18-signing-aax
# SDK-less build must be unchanged (no AAX_SDK_PATH):
cmake --build build-release -j   # formats AU;VST3;Standalone, no AAX target, no new warnings
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/melechdsp-hq/shared/dsp_bench && source .venv/bin/activate
PLUGIN=/Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter/build-release/MasterLimiter_artefacts/Release/VST3/MasterLimiter.vst3
for SLICE in 3 4 5; do python bench.py --driver master_limiter --slice $SLICE --quick --plugin-path "$PLUGIN" --output-dir "runs/SLICE18_CLOSE_S$SLICE"; done
bash -n scripts/sign_and_notarize.sh scripts/release_sign_macos.sh scripts/wraptool_sign_aax.sh
```
Gate: SDK-less build clean; Slice 3/4/5 PASS; scripts parse. STOP on failure.

────────────────────────────────────────
2. COMMITS (explicit paths)
────────────────────────────────────────

**Commit A — build + signing tooling:**
```bash
git add .gitignore CMakeLists.txt \
        scripts/release_sign_macos.sh scripts/sign_and_notarize.sh \
        scripts/wraptool_sign_aax.sh scripts/.aax_wraptool.env.example \
        scripts/source_repo_env.sh scripts/build_release.sh \
        scripts/create_installer.sh scripts/release_macos.sh \
        resources/MasterLimiter-standalone.entitlements
git commit -m "MasterLimiter Slice 18: code-signing + notarization + conditional AAX build + PACE wraptool + build/installer scripts"
```
Verify the real `scripts/.env` and `scripts/.aax_wraptool.env` are NOT in the
commit (only the `.aax_wraptool.env.example` template is committed).

**Commit B — docs + manual (write §3 PROGRESS/PLAN first):**
```bash
git add docs/RELEASE_SIGNING.md docs/AAX_SIGNING_REQUEST_TEMPLATE.md docs/MANUAL.md \
        docs/PROGRESS.md PROMPTS/PLAN.md
git commit -m "docs: Slice 18 release-signing/AAX guide + user manual + PROGRESS/PLAN"
```

**Commit C — archive prompt:**
```bash
git add PROMPTS/SLICE_18_signing_notarize_aax.md \
        PROMPTS/SLICE_18_1_build_installer_scripts.md PROMPTS/SLICE_18_CLOSE.md
git commit -m "docs: archive Slice 18 prompts"
```

────────────────────────────────────────
3. DOCS CONTENT (write before Commit B)
────────────────────────────────────────

### `docs/PROGRESS.md` — new TOP entry
Slice 18 — distribution tooling (product-only; no DSP/param change; SDK-less
build + Slice 3/4/5 unchanged):
- **Conditional AAX** in CMake: with no `AAX_SDK_PATH`, formats stay
  `AU;VST3;Standalone` (existing builds untouched); set `-DAAX_SDK_PATH=…` and
  the AAX target is added via `juce_set_aax_sdk_path`. Optional
  `ENABLE_AAX_WRAPTOOL_SIGN` POST_BUILD hook calls `scripts/wraptool_sign_aax.sh`.
- macOS scripts ported from AnalyzerPro: signing (`sign_and_notarize.sh`,
  `release_sign_macos.sh`, `wraptool_sign_aax.sh`), build/installer
  (`source_repo_env.sh`, `build_release.sh`, `create_installer.sh`,
  `release_macos.sh`), `.aax_wraptool.env.example`, and
  `resources/MasterLimiter-standalone.entitlements`. Real `.env`/
  `.aax_wraptool.env` gitignored (local only).
- AAX build verified: `build_release.sh` (sourcing the local `.env` with
  `AAX_SDK_PATH`) builds `MasterLimiter.aaxplugin` — Slice 18's conditional-AAX
  path confirmed with the real SDK.
- Docs: `RELEASE_SIGNING.md`, `AAX_SIGNING_REQUEST_TEMPLATE.md`, and the beta
  `MANUAL.md`.
- Pending (local/external, not code): `AAX_SDK_PATH`, `scripts/.aax_wraptool.env`
  (iLok creds), and MasterLimiter's own Avid/PACE registration. Product GUID
  `75B5E420-5C80-11F1-9221-00505692C25A` — verify whether it is the wraptool
  wrap-config GUID (else obtain wcguid from Fusion / use customernumber+name).

### `PROMPTS/PLAN.md`
- Mark **Slice 18 ✅ Shipped** (distribution tooling). Manual shipped.
- Note remaining to beta: avishali builds the AAX with the SDK, registers/
  signs it (PACE), tests in Pro Tools; then cut the **signed + notarized beta
  build** (VST3 + AU, + AAX once signed) for the tester.
- Backlog unchanged (palette centralization, knob/slider re-draw, auto-release
  tuning, final ceiling stage, HQ dual-meter consolidation).

────────────────────────────────────────
4. PUSH
────────────────────────────────────────

```bash
git push origin slice-18-signing-aax
git checkout main && git pull --ff-only && git merge --ff-only slice-18-signing-aax && git push origin main
```
Record new product `main` SHA; confirm submodule unchanged.

────────────────────────────────────────
5. OUTPUT REQUIREMENTS
────────────────────────────────────────

1. Pre-flight (SDK-less build + Slice 3/4/5 PASS; scripts parse). 2. Three
   commit SHAs; confirm no real `.env`/credentials committed. 3. Push + FF
   main; new main SHA; submodule unchanged. 4. Final `git status --short`
   (clean). 5. PROGRESS/PLAN excerpts. 6. Open questions.

────────────────────────────────────────
6. AFTER CLOSE
────────────────────────────────────────

Tooling is on `main`. Remaining to beta is avishali's local/external work
(AAX SDK build, PACE registration + sign, Pro Tools test) + cutting the signed
beta build. No further slice required unless the AAX-with-SDK build needs a
CMake tweak (→ Slice 18.1).
