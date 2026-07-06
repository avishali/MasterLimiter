#!/usr/bin/env bash
# Wire versioned git hooks for this repo (run once per clone).
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
HOOKS_DIR="${ROOT}/scripts/hooks"

if [[ ! -f "${HOOKS_DIR}/pre-commit" ]]; then
    echo "ERROR: ${HOOKS_DIR}/pre-commit not found" >&2
    exit 1
fi

chmod +x "${HOOKS_DIR}/pre-commit"
chmod +x "${ROOT}/scripts/check_ui_ascii.sh"

git -C "$ROOT" config core.hooksPath scripts/hooks

echo "Installed git hooks: core.hooksPath=scripts/hooks"
echo "Pre-commit runs: scripts/check_ui_ascii.sh (UI ASCII gate)"
echo "Note: CMake PRE_BUILD gate still runs on every build even if hooks are skipped."
