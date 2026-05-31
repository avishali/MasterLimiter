# Cursor Task — Slice 18: code-signing + notarization + AAX (PACE) build & signing

Paste this entire file into Cursor as the task prompt. Do not edit it.

> **Distribution tooling — port AnalyzerPro's signing/notarization/AAX flow to
> MasterLimiter**, so avishali can sign + notarize macOS builds and build/sign/
> test an AAX (Pro Tools) plugin. Product-side only: CMake AAX enablement +
> scripts + `.gitignore` + docs. **No DSP/HQ/param change.** Critical: AAX must
> be **conditional on `AAX_SDK_PATH`** so existing SDK-less builds (dev/bench/
> audition) are unaffected. Branch off `main`. **Do NOT push.**

> **Ordering:** branch off `main` after Slice 17 closes (both touch
> `CMakeLists.txt`). If 17 is open, either run it first or expect a trivial
> CMake merge.

────────────────────────────────────────
0. SYSTEM RULES PREAMBLE
────────────────────────────────────────

- Product edits: `CMakeLists.txt`, new `scripts/*.sh`, `scripts/
  .aax_wraptool.env.example`, `.gitignore`, `docs/` (AAX/signing docs),
  `resources/MasterLimiter-standalone.entitlements` if needed.
- **No DSP/HQ/parameter change.** Secrets/credentials are NEVER committed
  (gitignore the real `.env`/`.aax_wraptool.env`).
- Branch `slice-18-signing-aax` off `main`. **Do NOT commit, do NOT push.**

────────────────────────────────────────
1. SOURCE TO PORT (read AnalyzerPro originals)
────────────────────────────────────────

Read and adapt from `../AnalyzerPro/`:
- `scripts/sign_and_notarize.sh` — codesign (Hardened Runtime + entitlements
  for the Standalone) + `xcrun notarytool submit` + `stapler staple`;
  `SIGN_AND_NOTARIZE_SKIP_AAX` env honored.
- `scripts/release_sign_macos.sh` — simple in-place `codesign` of all bundles.
- `scripts/wraptool_sign_aax.sh` — PACE wraptool AAX signing; sources
  `scripts/.aax_wraptool.env`; invoked as a CMake POST_BUILD on the AAX target.
- `scripts/.aax_wraptool.env.example` — the wraptool credential template.
- `CMakeLists.txt` AAX wiring: `AAX_SDK_PATH` cache var, `juce_set_aax_sdk_path`,
  `ENABLE_AAX_WRAPTOOL_SIGN` option + `AAX_WRAPTOOL` filepath + the
  `add_custom_command(TARGET ${PLUGIN_NAME}_AAX POST_BUILD …)` wraptool step
  (AnalyzerPro CMake ~lines 46, 64–66, 185–193, 372, 473–490).
- `docs/aax_pace.md`, `docs/AAX_SIGNING_REQUEST_TEMPLATE.md`,
  `docs/DISTRIBUTING_UNSIGNED_BUILDS.md` — reference/adapt.

Output the retrieval log.

────────────────────────────────────────
2. CMAKE — enable AAX (CONDITIONAL on AAX_SDK_PATH)
────────────────────────────────────────

- Add `set(AAX_SDK_PATH "" CACHE PATH "Path to AAX SDK root (contains
  Interfaces/ACF)")`; also accept `$ENV{AAX_SDK_PATH}` if the cache var is empty.
- **Only when `AAX_SDK_PATH` is non-empty:** add `AAX` to `PLUGIN_FORMATS` and
  call `juce_set_aax_sdk_path("${AAX_SDK_PATH}")`. When it is empty, formats
  stay `AU;VST3;Standalone` exactly as today — **SDK-less builds must be
  unchanged** (dev/bench/audition keep working without the AAX SDK). Print a
  STATUS line for which case applies.
- Add `option(ENABLE_AAX_WRAPTOOL_SIGN ... OFF)` + `set(AAX_WRAPTOOL "" CACHE
  FILEPATH ...)` and, when `APPLE AND ENABLE_AAX_WRAPTOOL_SIGN AND PLUGIN_FORMATS
  matches AAX AND TARGET ${PLUGIN_NAME}_AAX`, the POST_BUILD `add_custom_command`
  calling `scripts/wraptool_sign_aax.sh "$<TARGET_FILE:${PLUGIN_NAME}_AAX>"
  "${AAX_WRAPTOOL}"` — mirror AnalyzerPro.
- Keep MasterLimiter's existing `PLUGIN_CODE`/`MANUFACTURER_CODE`
  (`MaLm`/`Melc`) and `BUNDLE_ID com.MelechDSP.MasterLimiter` — these give the
  AAX plugin its own IDs distinct from AnalyzerPro.

────────────────────────────────────────
3. SCRIPTS — port + adapt for MasterLimiter
────────────────────────────────────────

Create `scripts/` with adapted copies:
- `sign_and_notarize.sh` — set `PLUGIN_NAME=MasterLimiter`, `PLUGIN_VERSION=0.3.0`,
  keep the dev identity (`Developer ID Application/Installer: AVISHAY LIDANI
  (C5UC779LGC)`, `APPLE_ID=avishay.lidani@gmail.com`, `TEAM_ID=C5UC779LGC`,
  `NOTARYTOOL_KEYCHAIN_PROFILE=AC_PASSWORD`); point at MasterLimiter's
  `build-release/MasterLimiter_artefacts/Release`. Prefer making these
  overridable by env (with the AnalyzerPro values as defaults). Use a
  `resources/MasterLimiter-standalone.entitlements` (port the AnalyzerPro one).
- `release_sign_macos.sh` — `PLUGIN_NAME` default `MasterLimiter`; sign
  Standalone/.app, AU/.component, VST3/.vst3, and AAX/.aaxplugin when present.
- `wraptool_sign_aax.sh` — port as-is (it's product-agnostic; it takes the AAX
  Mach-O path and reads `.aax_wraptool.env`). Adjust only comments/paths.
- `.aax_wraptool.env.example` — port; keep the credential field names. The real
  `.aax_wraptool.env` is created locally by avishali and **gitignored**.

────────────────────────────────────────
4. GITIGNORE + DOCS
────────────────────────────────────────

- `.gitignore`: ensure `scripts/.aax_wraptool.env` and any `scripts/.env`
  (and other secret files) are ignored; the `.example` templates ARE committed.
- `docs/`: add a `RELEASE_SIGNING.md` (or port `aax_pace.md` +
  `DISTRIBUTING_UNSIGNED_BUILDS.md`) covering: the macOS sign+notarize+staple
  steps, the AAX build (`-DAAX_SDK_PATH=…`), the wraptool flow, and the
  **AAX caveat below**.

────────────────────────────────────────
5. AAX CAVEAT — document clearly
────────────────────────────────────────

In the docs, state plainly:
- **MasterLimiter needs its OWN Avid/PACE registration** — a MasterLimiter-
  specific wrap config GUID (or PACE customer-number + product). AnalyzerPro's
  `WCGUID`/product cannot be reused. Include the adapted
  `AAX_SIGNING_REQUEST_TEMPLATE` for requesting it from Avid (audiosdk@avid.com).
- **Unsigned AAX loads only in Avid developer builds of Pro Tools.** To test in
  retail Pro Tools, the AAX must be PACE-signed (wraptool, with the MasterLimiter
  wrap config + a signing iLok or the cloud signing service).

────────────────────────────────────────
6. BUILD & VERIFY
────────────────────────────────────────

```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter
git checkout -b slice-18-signing-aax
# (a) SDK-less build is unchanged:
cmake --build build-release -j           # still VST3/AU/Standalone, no AAX, no new warnings
# Slice 3/4/5 still PASS (bench builds don't set AAX_SDK_PATH)
# (b) AAX build (avishali, with the SDK):
#   cmake -B build-aax -DCMAKE_BUILD_TYPE=Release -DAAX_SDK_PATH=/path/to/aax-sdk-2-8-0 ...
#   cmake --build build-aax -j           # produces MasterLimiter.aaxplugin
```
- Confirm: with `AAX_SDK_PATH` empty, formats = AU;VST3;Standalone (unchanged),
  Slice 3/4/5 PASS. With it set, the AAX target appears.
- `scripts/release_sign_macos.sh` and `sign_and_notarize.sh` are executable and
  run (dry-run / on a built artifact) without syntax errors.
- `shellcheck` the scripts if available; report.

────────────────────────────────────────
7. OUTPUT REQUIREMENTS
────────────────────────────────────────

1. Retrieval log. 2. Diffs: CMake AAX (conditional) + wraptool option; the
   ported scripts; `.gitignore`; docs. 3. Build summary — confirm SDK-less
   build unchanged + Slice 3/4/5 PASS. 4. Note any env/credential/registration
   prerequisites avishali must supply locally (AAX_SDK_PATH, .aax_wraptool.env,
   Avid wrap config). 5. `git status --short`. 6. Open questions.

────────────────────────────────────────
8. ARCHITECT REVIEW + AVISHALI
────────────────────────────────────────

avishali: set `AAX_SDK_PATH`, build the AAX target, (register the product /
fill `.aax_wraptool.env`), wraptool-sign, and test in Pro Tools (dev build
unsigned, or signed). Report what works. On approval → architect closes
Slice 18. **Do not self-close.**
