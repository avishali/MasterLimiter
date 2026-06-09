# SLICE CLOSE — History Graph v2

**Status:** implemented locally; Release build clean; AU/VST3 installed; AU validation clean.
**Repo:** `MasterLimiter` only.
**Source prompt:** `PROMPTS/SLICE_HISTORY_GRAPH_V2.md`

## Delivered
- Extended `HistoryFrame` with `clipDb` and increased the ring to 65536 frames.
- Changed history cadence to ~0.5 ms and replaced block-held graph samples with
  per-sample-derived output, input, GR, and clip maxima.
- Added preallocated per-host-sample GR/clip scratch buffers, filled from the
  existing clipper attenuation and limiter total-gain paths.
- Added graph controls for 0.75/1.5/3/6/10/15/30 s windows, -24/-36/-48/-60 dB
  level floors, and 6/12/18/24 dB GR ranges.
- Added clip threshold line and red clip activity markers.
- Increased the default graph window size to 940 x 460.
- Fixed the clip LED repaint regression by invalidating the LED region in
  `syncMetersFromProcessor()`.
- Preserved the deferred history-window close fix (`MessageManager::callAsync`
  plus `Component::SafePointer`).

## RT Notes
- Per-sample scratch storage is allocated in `prepareToPlay()`.
- The audio callback does not allocate, lock, log, or perform file/OS I/O.
- Steady-state additions are scratch resets, min/max capture, dB conversion for
  published history samples, and one release-store per completed frame.

## Verification
- [x] Release build clean via `cmake --build build` (2026-06-09).
- [x] AU/VST3 copied to user plug-in folders.
- [x] AU validation clean via `auval -v aufx MaLm Melc`.
- [ ] Manual audition: smooth graph traces, long windows, range selectors, clip
  threshold/markers, and clip LED flashing.
