# Cursor Task — Slice 18.1: build + installer scripts (port from AnalyzerPro)

Paste this entire file into Cursor as the task prompt. Do not edit it.

> **Extends Slice 18 on the same branch** (`slice-18-signing-aax`). Ports the
> build + installer + orchestrator scripts from AnalyzerPro so MasterLimiter can
> build the AAX (via the local `scripts/.env`), then sign/notarize and produce a
> macOS installer. Committed scripts are parameterized; the local `.env` /
> `.aax_wraptool.env` (already created, gitignored) supply paths + credentials.
> **Do NOT commit, do NOT push** (the Slice 18 close commits everything).

────────────────────────────────────────
0. SYSTEM RULES PREAMBLE
────────────────────────────────────────

- Product scripts only: new `scripts/source_repo_env.sh`,
  `scripts/build_release.sh`, `scripts/create_installer.sh`,
  `scripts/release_macos.sh`; optional `docs/RELEASE_SIGNING.md` update. No DSP/
  HQ/param change.
- **Never commit secrets.** The local `scripts/.env` and
  `scripts/.aax_wraptool.env` exist and are gitignored — scripts source them;
  do not echo their contents into committed files.
- Continue branch `slice-18-signing-aax`. **Do NOT commit, do NOT push.**

────────────────────────────────────────
1. SOURCE TO PORT (AnalyzerPro)
────────────────────────────────────────

Read `../AnalyzerPro/scripts/`: `source_repo_env.sh`, `build_release.sh`,
`create_installer.sh`, `release_macos.sh`. Output the retrieval log.

────────────────────────────────────────
2. PORT + ADAPT
────────────────────────────────────────

Common adaptations: `PLUGIN_NAME=MasterLimiter`, `PLUGIN_VERSION=0.3.0`,
`COMPANY_NAME=MelechDSP` (fix AnalyzerPro's "MelecDSP" typo),
`BUNDLE_ID=com.MelechDSP.MasterLimiter`, rename `ANALYZERPRO_*` env vars →
`MASTERLIMITER_*`.

- **`source_repo_env.sh`** — port as-is (generic; sources `scripts/.env` when
  present; shell-exported vars win).
- **`build_release.sh`** — source `.env`, verify `JUCE_PATH`, then configure +
  build a Release. **MasterLimiter-specific:** pass
  `-DJUCE_PATH="$JUCE_PATH" -DMELECHDSP_HQ_ROOT="$MELECHDSP_HQ_ROOT"
  -DAAX_SDK_PATH="$AAX_SDK_PATH"` (the last enables the AAX target — Slice 18's
  conditional AAX). Build dir `build-release` (override via
  `MASTERLIMITER_RELEASE_BUILD_DIR`). (AnalyzerPro's didn't pass
  `MELECHDSP_HQ_ROOT` — MasterLimiter needs it.)
- **`create_installer.sh`** — adapt name/version/company; keep the AAX
  system-domain logic (AAX → `/Library/Application Support/Avid/Audio/Plug-Ins`
  when present; AU/VST3 per `INSTALL_DOMAIN`); produce the `.pkg`/`.dmg` under
  `installer/`. Rename the `ANALYZERPRO_*` knobs to `MASTERLIMITER_*`.
- **`release_macos.sh`** — orchestrate: `build_release.sh` →
  `wraptool_sign_aax.sh` (the built AAX Mach-O) → `SIGN_AND_NOTARIZE_SKIP_AAX=1
  sign_and_notarize.sh` (VST3/AU/Standalone; AAX already PACE-signed) →
  `create_installer.sh`.

────────────────────────────────────────
3. BUILD & VERIFY (this verifies Slice 18's AAX path with the real SDK)
────────────────────────────────────────

```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter
chmod +x scripts/*.sh
bash -n scripts/source_repo_env.sh scripts/build_release.sh scripts/create_installer.sh scripts/release_macos.sh
./scripts/build_release.sh        # sources .env (AAX_SDK_PATH set) → should build the AAX target
ls build-release/MasterLimiter_artefacts/Release/AAX/MasterLimiter.aaxplugin   # expect it to exist
```
- Gate: scripts parse; `build_release.sh` produces VST3/AU/Standalone **and**
  `MasterLimiter.aaxplugin` (this confirms Slice 18's conditional-AAX path
  compiles/links with the SDK). Zero new `Source/` warnings.
- Do NOT run wraptool/notarize/installer end-to-end here (needs iLok + PACE +
  the wrap-config GUID — that's avishali's runtime step). A dry `bash -n` of
  those is enough; report.
- If the AAX target fails to build, STOP and report the error (CMake/AAX-SDK
  fix needed before close).

────────────────────────────────────────
4. OUTPUT REQUIREMENTS
────────────────────────────────────────

1. Retrieval log. 2. The 4 ported scripts (diffs/new files). 3. Build result —
   **confirm `MasterLimiter.aaxplugin` built** (or the error). 4. Confirm no
   secrets committed; `.env`/`.aax_wraptool.env` remain gitignored/untracked.
5. `git status --short`. 6. Open questions.

────────────────────────────────────────
5. AFTER THIS
────────────────────────────────────────

The Slice 18 consolidated close (architect's, updated to include these scripts)
commits all distribution tooling. avishali then runs `release_macos.sh` (or the
steps individually) to build → PACE-sign AAX → notarize → installer, and tests
the AAX in Pro Tools. **Do not self-close.**
