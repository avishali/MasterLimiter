# Publish MasterLimiter beta to Cloudflare (R2 portal)

The beta portal is the sibling repo `../melechdsp-beta-portal` (Cloudflare Pages + Worker + private R2 `melechdsp-beta`). It already supports `PRODUCT=MasterLimiter`. Full reference: `melechdsp-beta-portal/docs/BETA_RELEASE_WORKFLOW.md`.

Version for this beta: **0.3.0-beta**.

---

## 1. Build the signed/notarized installer (.pkg)
In this repo (needs your PACE + Apple secrets — see `docs` / `scripts/.aax_wraptool.env` + notarytool profile `AC_PASSWORD`):
```bash
sudo rm -rf /Library/Audio/Plug-Ins/Components/MasterLimiter.component   # one-time, clears stale AU
bash scripts/release_macos_pkg.sh        # build → PACE-sign AAX → notarize → .pkg installer
```
Note the produced `.pkg` path. Rename/copy it to the portal's expected name:
```bash
cp <produced>.pkg MasterLimiter-0.3.0-beta.pkg
```

## 2. Verify before upload
```bash
shasum -a 256 MasterLimiter-0.3.0-beta.pkg | awk '{print $1}' > MasterLimiter-0.3.0-beta.pkg.sha256
spctl -a -vvv -t install MasterLimiter-0.3.0-beta.pkg     # should say: accepted, notarized
xcrun stapler validate MasterLimiter-0.3.0-beta.pkg        # should validate
# AAX: load in Pro Tools and confirm it authorizes (PACE)
```

## 3. Fill the manifest
Edit `release/beta.json` — replace **`sha256`** (both places) with the 64-hex digest from step 2; bump `build`/`date` if re-releasing. (`download_path`/`release_notes_path` are already correct for 0.3.0-beta.)

## 4. Upload (from the portal repo)
`wrangler` must be authed to Cloudflare (the portal's `.env` has `CLOUDFLARE_ACCOUNT_ID`; auth via `wrangler login` or `CLOUDFLARE_API_TOKEN`). `wrangler` is in `melechdsp-beta-portal/node_modules/.bin`.
```bash
cd ../melechdsp-beta-portal
BUCKET=melechdsp-beta PRODUCT=MasterLimiter PLATFORM=macOS \
  ./scripts/upload-release.sh \
  0.3.0-beta \
  /abs/path/MasterLimiter-0.3.0-beta.pkg \
  /abs/path/MasterLimiter-0.3.0-beta.pkg.sha256 \
  ../MasterLimiter/release/release-notes-0.3.0-beta.md \
  ../MasterLimiter/release/beta.json
```
This uploads (with `--remote`, i.e. the real bucket):
- `MasterLimiter/macOS/0.3.0-beta/MasterLimiter-0.3.0-beta.pkg`
- `…/MasterLimiter-0.3.0-beta.pkg.sha256`
- `…/release-notes.md`
- `manifests/MasterLimiter/macOS/beta.json`

## 5. Smoke test (portal)
Per `melechdsp-beta-portal/docs/LIVE_CHECKS.md`:
- Sign in via Cloudflare Access → open `/beta/downloads`.
- Confirm version/build/date/SHA-256 + release notes show MasterLimiter 0.3.0-beta.
- Generate a download link, download, confirm the SHA matches and the link expires after TTL.

## 6. Add testers + send
- `melechdsp-beta-portal/docs/ADD_TESTER.md` — grant the 1–2 testers Cloudflare Access.
- Send them the portal link + `docs/BETA_TESTER_GUIDE.md` (from this repo).

---
**Inputs prepared in `release/`:** `release-notes-0.3.0-beta.md`, `beta.json` (fill SHA). The `.pkg` is built locally and is gitignored.
