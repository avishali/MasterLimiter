# SLICE CLOSE — History Graph Window (Pro-L 2 style)

**Status:** implemented locally; Release build clean; AU/VST3 installed for audition.
**Repo:** `MasterLimiter` only.
**Source prompt:** `PROMPTS/SLICE_HISTORY_GRAPH.md`

## Delivered
- Added a processor-owned 4096-frame history ring for gain reduction, output
  peak, and input peak snapshots at a uniform ~2 ms sample cadence.
- Added `HistoryGraphComponent`, a self-contained UI component with 60 Hz ring
  draining, 1.5/3/6 s local time windows, per-pixel max decimation, dB/time
  grids, ceiling line, output fill, input line, and top-hanging GR fill.
- Added an editor-owned non-modal `DocumentWindow` opened by the main header
  `Graph` button.
- Registered the new component source in CMake.
- Updated `docs/SIGNAL_FLOW.md`, `docs/PROGRESS.md`, and `PROMPTS/PLAN.md`.

## RT Notes
- The audio callback adds only scalar max/compare work and a release-store when
  publishing a completed history frame.
- No locks, heap allocation, or UI-owned data access were added to the audio
  thread.

## Verification
- [x] Release build clean via `cmake --build build` (2026-06-09).
- [x] AU/VST3 copied to user plug-in folders.
- [ ] Manual audition: graph opens/closes repeatedly, scrolls live, and tracks
  GR/output/input behavior accurately across all time windows.
