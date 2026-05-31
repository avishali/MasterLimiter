# MasterLimiter Release Signing

This guide covers macOS Developer ID signing, notarization, and optional AAX
build/signing for MasterLimiter.

## macOS Signing and Notarization

Build release artifacts first:

```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/MasterLimiter
./scripts/build_release.sh
```

`scripts/build_release.sh` sources `scripts/.env` when present. Keep local paths
there, or export them in the shell:

```bash
export JUCE_PATH="/path/to/JUCE"
export MELECHDSP_HQ_ROOT="/Users/avishaylidani/DEV/GitHubRepo/MelechDSP/melechdsp-hq"
export AAX_SDK_PATH="/path/to/aax-sdk-2-8-0"
```

Sign bundles in place when you only need signed local artifacts:

```bash
./scripts/release_sign_macos.sh
```

Create signed distribution archives and notarize the DMG:

```bash
./scripts/sign_and_notarize.sh
```

Create an unsigned installer package/ZIP/DMG from the built artifacts:

```bash
./scripts/create_installer.sh
```

Run the full local pipeline when AAX credentials and Apple signing are available:

```bash
./scripts/release_macos.sh
```

Defaults can be overridden from the environment:

```bash
export DEVELOPER_ID_APP="Developer ID Application: AVISHAY LIDANI (C5UC779LGC)"
export DEVELOPER_ID_INSTALLER="Developer ID Installer: AVISHAY LIDANI (C5UC779LGC)"
export APPLE_ID="avishay.lidani@gmail.com"
export TEAM_ID="C5UC779LGC"
export NOTARYTOOL_KEYCHAIN_PROFILE="AC_PASSWORD"
export MASTERLIMITER_RELEASE_BUILD_DIR="build-release"
```

The standalone app is signed with
`resources/MasterLimiter-standalone.entitlements` so Hardened Runtime permits
audio input and JUCE/third-party library loading.

## AAX Build

AAX is opt-in. SDK-less builds keep the normal formats and do not require the AAX
SDK. To build AAX, configure with an AAX SDK root that contains `Interfaces/ACF`:

```bash
cmake -B build-aax -DCMAKE_BUILD_TYPE=Release -DAAX_SDK_PATH=/path/to/aax-sdk-2-8-0
cmake --build build-aax -j
```

You can also set `AAX_SDK_PATH` in the environment before configuring. When the
path is absent, MasterLimiter builds `AU;VST3;Standalone` only. When it is
present, CMake appends `AAX` and calls JUCE's AAX SDK setup.

`scripts/build_release.sh` passes `AAX_SDK_PATH` through to CMake so the local
`scripts/.env` can enable the AAX target for release builds.

## AAX PACE Signing

Retail Pro Tools requires PACE-signed AAX bundles. Unsigned AAX builds load only
in Avid developer builds of Pro Tools.

MasterLimiter needs its own Avid/PACE registration. Do not reuse AnalyzerPro's
`WCGUID`, wrap config, product, or signing approval. Request a MasterLimiter
wrap config GUID or a PACE customer-number/product mapping before retail Pro
Tools testing.

Set local wraptool secrets outside Git:

```bash
cp scripts/.aax_wraptool.env.example scripts/.aax_wraptool.env
$EDITOR scripts/.aax_wraptool.env
```

Then sign a built AAX bundle:

```bash
./scripts/wraptool_sign_aax.sh \
  build-aax/MasterLimiter_artefacts/Release/AAX/MasterLimiter.aaxplugin/Contents/MacOS/MasterLimiter
```

Or enable the CMake post-build step:

```bash
cmake -B build-aax \
  -DCMAKE_BUILD_TYPE=Release \
  -DAAX_SDK_PATH=/path/to/aax-sdk-2-8-0 \
  -DENABLE_AAX_WRAPTOOL_SIGN=ON \
  -DAAX_WRAPTOOL=/Applications/PACEAntiPiracy/Eden/Fusion/Current/bin/wraptool
cmake --build build-aax -j
```

If the AAX bundle was already PACE-signed, skip Apple re-signing during the final
distribution pass:

```bash
SIGN_AND_NOTARIZE_SKIP_AAX=1 ./scripts/sign_and_notarize.sh
```

## Local Prerequisites

- `AAX_SDK_PATH`: Avid AAX SDK root for AAX compilation.
- `scripts/.env`: local build paths such as `JUCE_PATH`, `MELECHDSP_HQ_ROOT`,
  `AAX_SDK_PATH`, and optional `MASTERLIMITER_RELEASE_BUILD_DIR`.
- `scripts/.aax_wraptool.env`: local wraptool account, password, signing ID, and
  MasterLimiter-specific publisher config.
- A PACE signing iLok or PACE cloud signing service access.
- A notarytool keychain profile matching `NOTARYTOOL_KEYCHAIN_PROFILE`.

See `docs/AAX_SIGNING_REQUEST_TEMPLATE.md` for the Avid/PACE request template.
