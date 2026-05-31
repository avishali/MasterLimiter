#!/usr/bin/env bash

# Installer Creation Script for MasterLimiter
# Creates distributable installer packages for macOS.
#
# INSTALL_DOMAIN=user|system - if unset and the build includes AAX, defaults to system so AAX
# installs to /Library/Application Support/Avid/Audio/Plug-Ins (Avid machine-wide path).
# Set INSTALL_DOMAIN=user to keep AU/VST3/AAX under the installing user's home Library instead.
# MASTERLIMITER_AAX_USE_SYSTEM_WIDE_LIBRARY=0 - when INSTALL_DOMAIN is unset, default user-only even if AAX exists.

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$PROJECT_ROOT"

# shellcheck source=/dev/null
source "$SCRIPT_DIR/source_repo_env.sh"

PLUGIN_NAME="MasterLimiter"
PLUGIN_VERSION="0.3.0"
COMPANY_NAME="MelechDSP"
BUNDLE_ID="com.MelechDSP.MasterLimiter"
BUILD_DIR="${MASTERLIMITER_RELEASE_BUILD_DIR:-build-release}"
ARTIFACTS_DIR="$BUILD_DIR/${PLUGIN_NAME}_artefacts/Release"
INSTALLER_DIR="$PROJECT_ROOT/installer"
TEMP_DIR="$INSTALLER_DIR/temp"
PAYLOAD_DIR="$TEMP_DIR/payload"

if [ ! -d "$ARTIFACTS_DIR" ]; then
    echo "Error: release artifacts not found: $ARTIFACTS_DIR"
    echo "Please run ./scripts/build_release.sh first"
    exit 1
fi

HAVE_AAX=0
if [ -d "$ARTIFACTS_DIR/AAX/${PLUGIN_NAME}.aaxplugin" ]; then
    HAVE_AAX=1
fi

MASTERLIMITER_AAX_USE_SYSTEM_WIDE_LIBRARY="${MASTERLIMITER_AAX_USE_SYSTEM_WIDE_LIBRARY:-1}"

if [ "${INSTALL_DOMAIN+x}" != x ]; then
    case "$INSTALL_DOMAIN" in
        user|system) ;;
        *)
            echo "Error: INSTALL_DOMAIN must be 'user' or 'system' (got '$INSTALL_DOMAIN')"
            exit 1
            ;;
    esac
elif [ "$HAVE_AAX" -eq 1 ] && [ "$MASTERLIMITER_AAX_USE_SYSTEM_WIDE_LIBRARY" != "0" ]; then
    INSTALL_DOMAIN=system
else
    INSTALL_DOMAIN=user
fi

if [ "$INSTALL_DOMAIN" = "user" ] && [ "$HAVE_AAX" -eq 1 ]; then
    echo "Warning: INSTALL_DOMAIN=user; AAX will install under ~/Library/Application Support/Avid/..."
    echo "For /Library/Application Support/Avid/Audio/Plug-Ins use INSTALL_DOMAIN=system or unset INSTALL_DOMAIN."
    echo ""
fi

echo "=========================================="
echo "  MasterLimiter Installer Creator"
echo "=========================================="
echo "  Version: $PLUGIN_VERSION"
echo "  Install domain: $INSTALL_DOMAIN (user -> ~/Library; system -> /Library / /Applications)"
if [ "$HAVE_AAX" -eq 1 ] && [ "$INSTALL_DOMAIN" = "system" ]; then
    echo "  AAX -> /Library/Application Support/Avid/Audio/Plug-Ins/ (system-wide; admin install)"
fi
echo "=========================================="
echo ""

echo "Preparing installer directories..."
mkdir -p "$INSTALLER_DIR"
rm -rf "$TEMP_DIR"
mkdir -p "$PAYLOAD_DIR"

DEST_COMPONENTS="Library/Audio/Plug-Ins/Components"
DEST_VST3="Library/Audio/Plug-Ins/VST3"
DEST_APPS="Applications"
DEST_AAX="Library/Application Support/Avid/Audio/Plug-Ins"
PKG_INSTALL_ROOT="/"

if [ "$INSTALL_DOMAIN" = "system" ]; then
    DOMAIN_LINE='<domains enable_anywhere="false" enable_currentUserHome="false" enable_localSystem="true"/>'
    DOC_PATH_AU="/Library/Audio/Plug-Ins/Components/"
    DOC_PATH_VST3="/Library/Audio/Plug-Ins/VST3/"
    DOC_PATH_APP="/Applications/"
    DOC_PATH_AAX="/Library/Application Support/Avid/Audio/Plug-Ins/"
else
    DOMAIN_LINE='<domains enable_anywhere="false" enable_currentUserHome="true" enable_localSystem="false"/>'
    DOC_PATH_AU="~/Library/Audio/Plug-Ins/Components/"
    DOC_PATH_VST3="~/Library/Audio/Plug-Ins/VST3/"
    DOC_PATH_APP="~/Applications/"
    DOC_PATH_AAX="~/Library/Application Support/Avid/Audio/Plug-Ins/"
fi

mkdir -p "$PAYLOAD_DIR/$DEST_COMPONENTS"
mkdir -p "$PAYLOAD_DIR/$DEST_VST3"
mkdir -p "$PAYLOAD_DIR/$DEST_APPS"
mkdir -p "$PAYLOAD_DIR/$DEST_AAX"

echo ""
echo "Packaging plugins..."
echo ""

if [ -d "$ARTIFACTS_DIR/AU/${PLUGIN_NAME}.component" ]; then
    echo "  Copying AU"
    cp -R "$ARTIFACTS_DIR/AU/${PLUGIN_NAME}.component" "$PAYLOAD_DIR/$DEST_COMPONENTS/"
fi

if [ -d "$ARTIFACTS_DIR/VST3/${PLUGIN_NAME}.vst3" ]; then
    echo "  Copying VST3"
    cp -R "$ARTIFACTS_DIR/VST3/${PLUGIN_NAME}.vst3" "$PAYLOAD_DIR/$DEST_VST3/"
fi

if [ -d "$ARTIFACTS_DIR/Standalone/${PLUGIN_NAME}.app" ]; then
    echo "  Copying Standalone"
    cp -R "$ARTIFACTS_DIR/Standalone/${PLUGIN_NAME}.app" "$PAYLOAD_DIR/$DEST_APPS/"
fi

if [ -d "$ARTIFACTS_DIR/AAX/${PLUGIN_NAME}.aaxplugin" ]; then
    echo "  Copying AAX (Pro Tools)"
    cp -R "$ARTIFACTS_DIR/AAX/${PLUGIN_NAME}.aaxplugin" "$PAYLOAD_DIR/$DEST_AAX/"
fi

echo ""
echo "Plugins packaged"
echo ""

README_SYS_NOTE=""
if [ "$INSTALL_DOMAIN" = "system" ]; then
    README_SYS_NOTE="Administrator password required. Plugins install to the system Library and /Applications (AAX: /Library/Application Support/Avid/Audio/Plug-Ins).

"
fi

cat > "$PAYLOAD_DIR/README.txt" << EOF
${PLUGIN_NAME} v${PLUGIN_VERSION}
${COMPANY_NAME}

INSTALLATION
============

${README_SYS_NOTE}Use the Installer "Customize" step to choose formats. Default install locations:

1. AU (Audio Unit): ${DOC_PATH_AU}
2. VST3: ${DOC_PATH_VST3}
3. Standalone App: ${DOC_PATH_APP}
4. AAX (Pro Tools, if included): ${DOC_PATH_AAX}

Use a PACE-signed AAX build before creating this installer (see scripts/wraptool_sign_aax.sh and docs/RELEASE_SIGNING.md).

SYSTEM REQUIREMENTS
===================

- macOS 10.13 or later
- Intel or Apple Silicon Mac
- Compatible DAW (for AU/VST3/AAX plugins)

SUPPORT
=======

For support, visit: https://www.melechdsp.com

Copyright (c) $(date +%Y) ${COMPANY_NAME}. All rights reserved.
EOF

echo "Creating macOS installer package (use Customize to select AU / VST3 / Standalone / AAX)..."
echo ""

PKG_OUTPUT="$INSTALLER_DIR/${PLUGIN_NAME}-${PLUGIN_VERSION}-macOS.pkg"
rm -f "$PKG_OUTPUT"
SUBROOTS="$TEMP_DIR/subroots"
rm -rf "$SUBROOTS"
mkdir -p "$SUBROOTS"

write_one_bundle_plist() {
  local plist_out="$1"
  local rel_path="$2"
  cat > "$plist_out" << PLISTEOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<array>
    <dict>
        <key>BundleIsRelocatable</key>
        <false/>
        <key>BundleIsVersionChecked</key>
        <false/>
        <key>BundleHasStrictIdentifier</key>
        <false/>
        <key>BundleOverwriteAction</key>
        <string>upgrade</string>
        <key>RootRelativeBundlePath</key>
        <string>${rel_path}</string>
    </dict>
</array>
</plist>
PLISTEOF
}

build_subpkg() {
  local format_id="$1"
  local pkg_file="$2"
  local dest_subdir="$3"
  local bundle_name="$4"
  local source_path="$5"
  local rel_in_pkg="${dest_subdir}/${bundle_name}"
  local package_id="${BUNDLE_ID}.${format_id}"

  local root="${SUBROOTS}/${format_id}"
  mkdir -p "${root}/${dest_subdir}"
  cp -R "${source_path}" "${root}/${dest_subdir}/"

  local plist="${TEMP_DIR}/component-${format_id}.plist"
  write_one_bundle_plist "${plist}" "${rel_in_pkg}"

  pkgbuild \
    --root "${root}" \
    --identifier "${package_id}" \
    --version "${PLUGIN_VERSION}" \
    --install-location "${PKG_INSTALL_ROOT}" \
    --component-plist "${plist}" \
    --min-os-version "10.13" \
    "${TEMP_DIR}/${pkg_file}"
}

SUBPKG_COUNT=0
if [ -d "$ARTIFACTS_DIR/AU/${PLUGIN_NAME}.component" ]; then
  echo "  Flat package: AU"
  build_subpkg "au" "${PLUGIN_NAME}-AU.pkg" "${DEST_COMPONENTS}" "${PLUGIN_NAME}.component" "$ARTIFACTS_DIR/AU/${PLUGIN_NAME}.component"
  SUBPKG_COUNT=$((SUBPKG_COUNT + 1))
fi
if [ -d "$ARTIFACTS_DIR/VST3/${PLUGIN_NAME}.vst3" ]; then
  echo "  Flat package: VST3"
  build_subpkg "vst3" "${PLUGIN_NAME}-VST3.pkg" "${DEST_VST3}" "${PLUGIN_NAME}.vst3" "$ARTIFACTS_DIR/VST3/${PLUGIN_NAME}.vst3"
  SUBPKG_COUNT=$((SUBPKG_COUNT + 1))
fi
if [ -d "$ARTIFACTS_DIR/Standalone/${PLUGIN_NAME}.app" ]; then
  echo "  Flat package: Standalone"
  build_subpkg "standalone" "${PLUGIN_NAME}-Standalone.pkg" "${DEST_APPS}" "${PLUGIN_NAME}.app" "$ARTIFACTS_DIR/Standalone/${PLUGIN_NAME}.app"
  SUBPKG_COUNT=$((SUBPKG_COUNT + 1))
fi
if [ -d "$ARTIFACTS_DIR/AAX/${PLUGIN_NAME}.aaxplugin" ]; then
  echo "  Flat package: AAX"
  build_subpkg "aax" "${PLUGIN_NAME}-AAX.pkg" "${DEST_AAX}" "${PLUGIN_NAME}.aaxplugin" "$ARTIFACTS_DIR/AAX/${PLUGIN_NAME}.aaxplugin"
  SUBPKG_COUNT=$((SUBPKG_COUNT + 1))
fi

if [ "$SUBPKG_COUNT" -eq 0 ]; then
  echo "No plugin formats found under $ARTIFACTS_DIR; nothing to package."
  exit 1
fi

DIST_XML="${TEMP_DIR}/Distribution.xml"
{
  echo '<?xml version="1.0" encoding="utf-8"?>'
  echo '<installer-gui-script minSpecVersion="2">'
  echo "  <title>${PLUGIN_NAME} ${PLUGIN_VERSION}</title>"
  echo '  <options customize="always" require-scripts="false"/>'
  echo "  ${DOMAIN_LINE}"
  echo '  <choices-outline>'
  [ -f "${TEMP_DIR}/${PLUGIN_NAME}-AU.pkg" ] && echo '    <line choice="fmt_au"/>'
  [ -f "${TEMP_DIR}/${PLUGIN_NAME}-VST3.pkg" ] && echo '    <line choice="fmt_vst3"/>'
  [ -f "${TEMP_DIR}/${PLUGIN_NAME}-Standalone.pkg" ] && echo '    <line choice="fmt_standalone"/>'
  [ -f "${TEMP_DIR}/${PLUGIN_NAME}-AAX.pkg" ] && echo '    <line choice="fmt_aax"/>'
  echo '  </choices-outline>'
  if [ -f "${TEMP_DIR}/${PLUGIN_NAME}-AU.pkg" ]; then
    printf '  <choice id="fmt_au" visible="true" title="Audio Unit (AU)" description="Logic, GarageBand, and other AU hosts. Installs to: %s" start_selected="true">\n' "${DOC_PATH_AU}"
    echo "    <pkg-ref id=\"${BUNDLE_ID}.au\"/>"
    echo '  </choice>'
  fi
  if [ -f "${TEMP_DIR}/${PLUGIN_NAME}-VST3.pkg" ]; then
    printf '  <choice id="fmt_vst3" visible="true" title="VST3" description="Ableton, Cubase, Reaper, and other VST3 hosts. Installs to: %s" start_selected="true">\n' "${DOC_PATH_VST3}"
    echo "    <pkg-ref id=\"${BUNDLE_ID}.vst3\"/>"
    echo '  </choice>'
  fi
  if [ -f "${TEMP_DIR}/${PLUGIN_NAME}-Standalone.pkg" ]; then
    printf '  <choice id="fmt_standalone" visible="true" title="Standalone application" description="MasterLimiter app. Installs to: %s" start_selected="true">\n' "${DOC_PATH_APP}"
    echo "    <pkg-ref id=\"${BUNDLE_ID}.standalone\"/>"
    echo '  </choice>'
  fi
  if [ -f "${TEMP_DIR}/${PLUGIN_NAME}-AAX.pkg" ]; then
    printf '  <choice id="fmt_aax" visible="true" title="AAX (Pro Tools)" description="Pro Tools and AAX hosts. PACE-signed build required. Installs to: %s" start_selected="true">\n' "${DOC_PATH_AAX}"
    echo "    <pkg-ref id=\"${BUNDLE_ID}.aax\"/>"
    echo '  </choice>'
  fi
  [ -f "${TEMP_DIR}/${PLUGIN_NAME}-AU.pkg" ] && echo "  <pkg-ref id=\"${BUNDLE_ID}.au\" version=\"${PLUGIN_VERSION}\" onConclusion=\"none\">${PLUGIN_NAME}-AU.pkg</pkg-ref>"
  [ -f "${TEMP_DIR}/${PLUGIN_NAME}-VST3.pkg" ] && echo "  <pkg-ref id=\"${BUNDLE_ID}.vst3\" version=\"${PLUGIN_VERSION}\" onConclusion=\"none\">${PLUGIN_NAME}-VST3.pkg</pkg-ref>"
  [ -f "${TEMP_DIR}/${PLUGIN_NAME}-Standalone.pkg" ] && echo "  <pkg-ref id=\"${BUNDLE_ID}.standalone\" version=\"${PLUGIN_VERSION}\" onConclusion=\"none\">${PLUGIN_NAME}-Standalone.pkg</pkg-ref>"
  [ -f "${TEMP_DIR}/${PLUGIN_NAME}-AAX.pkg" ] && echo "  <pkg-ref id=\"${BUNDLE_ID}.aax\" version=\"${PLUGIN_VERSION}\" onConclusion=\"none\">${PLUGIN_NAME}-AAX.pkg</pkg-ref>"
  echo '</installer-gui-script>'
} > "${DIST_XML}"

if ! productbuild \
    --distribution "${DIST_XML}" \
    --package-path "${TEMP_DIR}" \
    "${PKG_OUTPUT}"; then
  echo "Package creation failed"
  exit 1
fi

echo ""
echo "Installer package created"
echo ""

echo "Creating ZIP archive..."
ZIP_OUTPUT="$INSTALLER_DIR/${PLUGIN_NAME}-${PLUGIN_VERSION}-macOS.zip"
rm -f "$ZIP_OUTPUT"

cd "$PAYLOAD_DIR"
zip -r -q "$ZIP_OUTPUT" .
cd "$PROJECT_ROOT"

echo "ZIP archive created"
echo ""

echo "Creating DMG disk image..."
DMG_OUTPUT="$INSTALLER_DIR/${PLUGIN_NAME}-${PLUGIN_VERSION}-macOS.dmg"
DMG_TEMP="$TEMP_DIR/dmg"
rm -f "$DMG_OUTPUT"

mkdir -p "$DMG_TEMP"
cp -R "$PAYLOAD_DIR/"* "$DMG_TEMP/"

cat > "$DMG_TEMP/INSTALL.txt" << EOF
${PLUGIN_NAME} v${PLUGIN_VERSION}

INSTALLATION INSTRUCTIONS
=========================

1. Copy ${PLUGIN_NAME}.component to:
   ${DOC_PATH_AU}

2. Copy ${PLUGIN_NAME}.vst3 to:
   ${DOC_PATH_VST3}

3. Copy ${PLUGIN_NAME}.app to:
   ${DOC_PATH_APP}

4. Copy ${PLUGIN_NAME}.aaxplugin (if present) to:
   ${DOC_PATH_AAX}

5. Restart your DAW

For easier installation, use the .pkg installer: click Customize to choose AU, VST3, Standalone, and/or AAX.

AAX must be PACE-signed for Pro Tools distribution; do not re-sign the inner binary with Apple after PACE (see docs/RELEASE_SIGNING.md).

---
${COMPANY_NAME} (c) $(date +%Y)
EOF

if hdiutil create \
    -volname "${PLUGIN_NAME} ${PLUGIN_VERSION}" \
    -srcfolder "$DMG_TEMP" \
    -ov \
    -format UDZO \
    "$DMG_OUTPUT" \
    2>/dev/null; then
    echo "DMG created"
else
    echo "DMG creation skipped (requires hdiutil)"
fi

echo ""
echo "Cleaning up..."
rm -rf "$TEMP_DIR"

echo ""
echo "=========================================="
echo "  Distribution Packages Created"
echo "=========================================="
echo ""
echo "Installer Package:"
echo "   $PKG_OUTPUT"
echo ""
echo "ZIP Archive:"
echo "   $ZIP_OUTPUT"
echo ""
if [ -f "$DMG_OUTPUT" ]; then
    echo "DMG Disk Image:"
    echo "   $DMG_OUTPUT"
    echo ""
fi
echo "=========================================="
echo ""
echo "IMPORTANT: Code Signing, Notarization, and AAX"
echo "=========================================="
echo ""
echo "AAX, if packaged, must be PACE-signed with MasterLimiter's own Avid/PACE registration."
echo "Use SIGN_AND_NOTARIZE_SKIP_AAX=1 when running scripts/sign_and_notarize.sh after PACE signing."
echo ""
echo "Installer creation complete"
echo ""
