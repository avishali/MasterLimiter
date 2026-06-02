#!/usr/bin/env bash

# Code signing and notarization script for MasterLimiter macOS distribution.
# Adapted from the AnalyzerPro pipeline: signs the bundles, rebuilds the installer,
# productsigns the .pkg, submits the .pkg to notarytool, then staples it. Also
# emits signed zip/dmg archives. The notarized, stapled .pkg is the deliverable.
#
# For AAX, run PACE wraptool first, then skip Apple re-signing of that bundle:
#   ./scripts/wraptool_sign_aax.sh \
#     build-release/MasterLimiter_artefacts/Release/AAX/MasterLimiter.aaxplugin/Contents/MacOS/MasterLimiter
#   SIGN_AND_NOTARIZE_SKIP_AAX=1 ./scripts/sign_and_notarize.sh

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$PROJECT_ROOT"

DEVELOPER_ID_APP="${DEVELOPER_ID_APP:-Developer ID Application: AVISHAY LIDANI (C5UC779LGC)}"
DEVELOPER_ID_INSTALLER="${DEVELOPER_ID_INSTALLER:-Developer ID Installer: AVISHAY LIDANI (C5UC779LGC)}"
APPLE_ID="${APPLE_ID:-avishay.lidani@gmail.com}"
TEAM_ID="${TEAM_ID:-C5UC779LGC}"
NOTARYTOOL_KEYCHAIN_PROFILE="${NOTARYTOOL_KEYCHAIN_PROFILE:-AC_PASSWORD}"

PLUGIN_NAME="${PLUGIN_NAME:-MasterLimiter}"
PLUGIN_VERSION="${PLUGIN_VERSION:-0.3.0}"
BUILD_DIR="${MASTERLIMITER_RELEASE_BUILD_DIR:-${BUILD_DIR:-build-release}}"
CONFIG="${CONFIG:-Release}"
ARTIFACTS_DIR="${BUILD_DIR}/${PLUGIN_NAME}_artefacts/${CONFIG}"
DIST_DIR="${DIST_DIR:-installer}"
STANDALONE_ENTITLEMENTS="${STANDALONE_ENTITLEMENTS:-$PROJECT_ROOT/resources/MasterLimiter-standalone.entitlements}"

mkdir -p "$DIST_DIR"
DIST_DIR="$(cd "$DIST_DIR" && pwd)"

if [[ ! -d "$ARTIFACTS_DIR" ]]; then
  echo "ERROR: Release artifacts not found: $ARTIFACTS_DIR"
  echo "Run a release build first, for example:"
  echo "  cmake --build build-release -j"
  exit 1
fi

echo "=========================================="
echo "  MasterLimiter Signing and Notarization"
echo "=========================================="
echo ""
echo "Artifacts: $ARTIFACTS_DIR"
echo "Apple ID:  $APPLE_ID"
echo "Team ID:   $TEAM_ID"
echo "Profile:   $NOTARYTOOL_KEYCHAIN_PROFILE"
echo "Installer identity: $DEVELOPER_ID_INSTALLER"
echo ""

sign_bundle() {
  local bundle_path="$1"
  local entitlements="${2:-}"
  local bundle_name

  bundle_name="$(basename "$bundle_path")"

  if [[ -n "$entitlements" ]]; then
    if [[ ! -f "$entitlements" ]]; then
      echo "ERROR: Entitlements file not found: $entitlements"
      exit 1
    fi
  fi

  echo "Signing: $bundle_name"
  if [[ -n "$entitlements" ]]; then
    codesign --deep --force --verify --verbose \
      --sign "$DEVELOPER_ID_APP" \
      --options runtime \
      --timestamp \
      --entitlements "$entitlements" \
      "$bundle_path"
  else
    codesign --deep --force --verify --verbose \
      --sign "$DEVELOPER_ID_APP" \
      --options runtime \
      --timestamp \
      "$bundle_path"
  fi

  codesign --verify --deep --strict --verbose=2 "$bundle_path"
  spctl --assess --verbose=4 --type install "$bundle_path" 2>&1 | sed -n '1,3p' || true
  echo ""
}

if [[ -d "$ARTIFACTS_DIR/AU/${PLUGIN_NAME}.component" ]]; then
  sign_bundle "$ARTIFACTS_DIR/AU/${PLUGIN_NAME}.component"
fi

if [[ -d "$ARTIFACTS_DIR/VST3/${PLUGIN_NAME}.vst3" ]]; then
  sign_bundle "$ARTIFACTS_DIR/VST3/${PLUGIN_NAME}.vst3"
fi

if [[ -d "$ARTIFACTS_DIR/Standalone/${PLUGIN_NAME}.app" ]]; then
  sign_bundle "$ARTIFACTS_DIR/Standalone/${PLUGIN_NAME}.app" "$STANDALONE_ENTITLEMENTS"
fi

if [[ -d "$ARTIFACTS_DIR/AAX/${PLUGIN_NAME}.aaxplugin" ]]; then
  if [[ "${SIGN_AND_NOTARIZE_SKIP_AAX:-}" == "1" ]]; then
    echo "Skipping Apple codesign on AAX (SIGN_AND_NOTARIZE_SKIP_AAX=1; use PACE-signed bundle)."
  else
    echo "Note: AAX requires additional MasterLimiter-specific PACE signing for retail Pro Tools."
    sign_bundle "$ARTIFACTS_DIR/AAX/${PLUGIN_NAME}.aaxplugin"
  fi
fi

echo "All plugins signed."
echo ""
echo "Building installer package from signed bundles..."
"$SCRIPT_DIR/create_installer.sh"

UNSIGNED_PKG="$DIST_DIR/${PLUGIN_NAME}-${PLUGIN_VERSION}-macOS.pkg"
SIGNED_PKG="$DIST_DIR/${PLUGIN_NAME}-${PLUGIN_VERSION}-macOS-signed.pkg"
SIGNED_ZIP="$DIST_DIR/${PLUGIN_NAME}-${PLUGIN_VERSION}-macOS-signed.zip"
SIGNED_DMG="$DIST_DIR/${PLUGIN_NAME}-${PLUGIN_VERSION}-macOS-signed.dmg"
TEMP_DMG_DIR="$(mktemp -d "/tmp/${PLUGIN_NAME}-dmg.XXXXXX")"
NOTARY_TMP="$(mktemp)"

cleanup() {
  rm -rf "$TEMP_DMG_DIR"
  rm -f "$NOTARY_TMP"
}
trap cleanup EXIT

if [[ ! -f "$UNSIGNED_PKG" ]]; then
  echo "ERROR: Expected installer not found: $UNSIGNED_PKG"
  exit 1
fi

echo ""
echo "Signing installer package..."
productsign --sign "$DEVELOPER_ID_INSTALLER" "$UNSIGNED_PKG" "$SIGNED_PKG"
pkgutil --check-signature "$SIGNED_PKG"

echo ""
echo "Submitting PKG for notarization (this may take several minutes)..."
set +e
(set -o pipefail; set -e
  xcrun notarytool submit "$SIGNED_PKG" \
    --keychain-profile "$NOTARYTOOL_KEYCHAIN_PROFILE" \
    --wait 2>&1 | tee "$NOTARY_TMP"
)
NOTARY_EXIT=$?
set -e

LAST_STATUS_LINE="$(grep -E '^[[:space:]]*status:[[:space:]]*' "$NOTARY_TMP" | tail -1 || true)"
if ! echo "$LAST_STATUS_LINE" | grep -qE 'status:[[:space:]]*Accepted'; then
  NOTARY_EXIT=1
fi

if [[ "$NOTARY_EXIT" -ne 0 ]]; then
  echo ""
  echo "ERROR: Notarization was not accepted. Stapling is skipped."
  REQUEST_ID="$(grep -E '[[:space:]]id:' "$NOTARY_TMP" | tail -1 | sed -E 's/^[[:space:]]*id:[[:space:]]*//' | tr -d '\r' || true)"
  if [[ -n "$REQUEST_ID" ]]; then
    echo ""
    echo "Apple notary log for submission $REQUEST_ID:"
    xcrun notarytool log "$REQUEST_ID" --keychain-profile "$NOTARYTOOL_KEYCHAIN_PROFILE" 2>&1 || true
    echo ""
    echo "To re-fetch later:"
    echo "  xcrun notarytool log $REQUEST_ID --keychain-profile $NOTARYTOOL_KEYCHAIN_PROFILE"
  fi
  exit 1
fi

echo ""
echo "Notarization accepted. Stapling PKG..."
xcrun stapler staple "$SIGNED_PKG"
xcrun stapler validate "$SIGNED_PKG"

echo ""
echo "Creating signed ZIP: $SIGNED_ZIP"
rm -f "$SIGNED_ZIP"
(cd "$ARTIFACTS_DIR" && zip -r -q "$SIGNED_ZIP" .)

echo "Creating signed DMG: $SIGNED_DMG"
rm -f "$SIGNED_DMG"
cp -R "$ARTIFACTS_DIR/"* "$TEMP_DMG_DIR/"
hdiutil create \
  -volname "${PLUGIN_NAME} ${PLUGIN_VERSION}" \
  -srcfolder "$TEMP_DMG_DIR" \
  -ov \
  -format UDZO \
  "$SIGNED_DMG"

echo ""
echo "Distribution complete:"
echo "  $SIGNED_PKG   (notarized + stapled — primary installer)"
echo "  $SIGNED_ZIP"
echo "  $SIGNED_DMG"
echo ""
echo "AAX prerequisite: retail Pro Tools requires PACE signing with MasterLimiter's own Avid/PACE registration."
