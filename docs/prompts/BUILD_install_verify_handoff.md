# Cursor handoff — BUILD + INSTALL + VERIFY (stale-binary fix)

**Repo:** `MasterLimiter`. HEAD = `608b447`. SDK unchanged. NOT a code change — a build/install
+ verification task.

## Why

The whole "crossover keeps crashing" investigation was chasing a **stale installed AU**. Ableton
loads `~/Library/Audio/Plug-Ins/Components/MasterLimiter.component`, which was timestamped
**03:17** — before ALL five crossover fixes (23b059c → 608b447). "Release build is fresh" built
to `build/` but the artefact never reached the loaded plugin folder (JUCE incremental build
silently skipped the relink/COPY-after-build; the root-owned `/Library` system copy is also
stale). So none of the fixes ran. We must produce a verified-fresh install before any more
debugging.

## Do this

1. **Clean Release build** (force a real relink — the silent-skip is the whole problem):
   - Remove stale build trees: `build/`, `build-release/` (whatever the project uses).
   - Configure + build Release (Standalone/AU/VST3).

2. **Install the artefact to the USER plugin folders** (what Ableton actually reads), overwriting
   the stale copies:
   - `~/Library/Audio/Plug-Ins/Components/MasterLimiter.component`
   - `~/Library/Audio/Plug-Ins/VST3/MasterLimiter.vst3`
   - The stale **root-owned** copies in `/Library/Audio/Plug-Ins/...` (04:54) can shadow/confuse
     scans — either remove them (needs `sudo`, ask avishali) or confirm Ableton is pointed at the
     user folder. Do NOT leave a newer user copy alongside an older system copy silently.

3. **VERIFY before handing back** — print all of:
   - `git rev-parse --short HEAD` (must be `608b447`).
   - `ls -la ~/Library/Audio/Plug-Ins/Components/MasterLimiter.component/Contents/MacOS/MasterLimiter`
     — mtime MUST be from this build (today, not 03:17).
   - `cmp` the freshly built artefact against the installed one → MUST be identical.
   - Re-sign if the project requires it for AU validation; run `auval -v aufx <subtype> <manu>`
     (or the project's validation) → must pass on the NEW binary.

4. **Bust the AU cache so Ableton reloads the new component:**
   - `killall -9 AudioComponentRegistrar` (and/or the project's usual rescan step).
   - Tell avishali to rescan plugins in Ableton (or toggle the plugin folder) before testing.

## Durable safeguard (do this too — small code change)
Embed the git short-hash in the plugin so the LOADED build is visible and this can never silently
recur:
- Pass `-DMASTERLIMITER_GIT_HASH=$(git rev-parse --short HEAD)` (or via `configure_file`) into the
  build, and append it to the version/about string shown in the DEV panel or plugin name
  (e.g. `MasterLimiter 0.3.x (608b447)`).
- Then any audition can confirm at a glance which commit is running.

## Report back
- The verification block (HEAD hash, installed mtime, `cmp` result, auval result).
- Confirm the user-folder AU is now the `608b447` build.
- Commit (if the git-hash safeguard added): `build: embed git hash in version + ensure artefact install`.
- Do NOT push (Quell hold).

## After this
avishali re-tests the crossover on the VERIFIED-fresh `608b447` AU (drag/step atten, cutoff,
transition, 60–90 s). Only if it STILL crashes do we go to ASan — the fixes have never actually
been loaded, so they're likely fine.
