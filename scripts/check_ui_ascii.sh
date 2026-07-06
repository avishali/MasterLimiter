#!/usr/bin/env bash
# Thin wrapper: run the canonical SDK check against this product's UI sources.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
UI_DIR="${ROOT}/Source/ui"

_resolve_hq_check() {
    local candidates=()

    if [[ -n "${MELECHDSP_HQ_ROOT:-}" ]]; then
        candidates+=("${MELECHDSP_HQ_ROOT}/tools/check_ui_ascii.sh")
        candidates+=("${MELECHDSP_HQ_ROOT}/melechdsp-hq/tools/check_ui_ascii.sh")
    fi

    candidates+=(
        "${ROOT}/third_party/melechdsp-hq/tools/check_ui_ascii.sh"
        "${ROOT}/../melechdsp-hq/tools/check_ui_ascii.sh"
    )

    local c
    for c in "${candidates[@]}"; do
        if [[ -f "$c" ]]; then
            echo "$c"
            return 0
        fi
    done

    return 1
}

CHECK="$(_resolve_hq_check || true)"
if [[ -z "$CHECK" ]]; then
    echo "ERROR: could not find melechdsp-hq/tools/check_ui_ascii.sh" >&2
    echo "Set MELECHDSP_HQ_ROOT or vendor melechdsp-hq under third_party/." >&2
    exit 2
fi

exec /bin/bash "$CHECK" "$UI_DIR"
