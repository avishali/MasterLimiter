#!/usr/bin/env bash
# Load scripts/.env when present (gitignored). Source from other scripts after SCRIPT_DIR
# is set to this directory (.../MasterLimiter/scripts).
#
# If known release variables were already set in the environment before sourcing,
# they win over .env.

if [[ -z "${SCRIPT_DIR:-}" ]]; then
  echo "source_repo_env.sh: SCRIPT_DIR must be set before sourcing" >&2
  return 1 2>/dev/null || exit 1
fi

if [[ ! -f "${SCRIPT_DIR}/.env" ]]; then
  return 0 2>/dev/null || exit 0
fi

if [[ "${JUCE_PATH+x}" == "x" ]]; then _repo_env_prev_JUCE_PATH="$JUCE_PATH"; else unset _repo_env_prev_JUCE_PATH; fi
if [[ "${MELECHDSP_HQ_ROOT+x}" == "x" ]]; then _repo_env_prev_MELECHDSP_HQ_ROOT="$MELECHDSP_HQ_ROOT"; else unset _repo_env_prev_MELECHDSP_HQ_ROOT; fi
if [[ "${AAX_SDK_PATH+x}" == "x" ]]; then _repo_env_prev_AAX_SDK_PATH="$AAX_SDK_PATH"; else unset _repo_env_prev_AAX_SDK_PATH; fi
if [[ "${MASTERLIMITER_RELEASE_BUILD_DIR+x}" == "x" ]]; then _repo_env_prev_MASTERLIMITER_RELEASE_BUILD_DIR="$MASTERLIMITER_RELEASE_BUILD_DIR"; else unset _repo_env_prev_MASTERLIMITER_RELEASE_BUILD_DIR; fi
if [[ "${INSTALL_DOMAIN+x}" == "x" ]]; then _repo_env_prev_INSTALL_DOMAIN="$INSTALL_DOMAIN"; else unset _repo_env_prev_INSTALL_DOMAIN; fi
if [[ "${MASTERLIMITER_AAX_USE_SYSTEM_WIDE_LIBRARY+x}" == "x" ]]; then _repo_env_prev_MASTERLIMITER_AAX_USE_SYSTEM_WIDE_LIBRARY="$MASTERLIMITER_AAX_USE_SYSTEM_WIDE_LIBRARY"; else unset _repo_env_prev_MASTERLIMITER_AAX_USE_SYSTEM_WIDE_LIBRARY; fi

set -a
# shellcheck source=/dev/null
source "${SCRIPT_DIR}/.env"
set +a

if [[ "${_repo_env_prev_JUCE_PATH+x}" == "x" ]]; then export JUCE_PATH="${_repo_env_prev_JUCE_PATH}"; unset _repo_env_prev_JUCE_PATH; fi
if [[ "${_repo_env_prev_MELECHDSP_HQ_ROOT+x}" == "x" ]]; then export MELECHDSP_HQ_ROOT="${_repo_env_prev_MELECHDSP_HQ_ROOT}"; unset _repo_env_prev_MELECHDSP_HQ_ROOT; fi
if [[ "${_repo_env_prev_AAX_SDK_PATH+x}" == "x" ]]; then export AAX_SDK_PATH="${_repo_env_prev_AAX_SDK_PATH}"; unset _repo_env_prev_AAX_SDK_PATH; fi
if [[ "${_repo_env_prev_MASTERLIMITER_RELEASE_BUILD_DIR+x}" == "x" ]]; then export MASTERLIMITER_RELEASE_BUILD_DIR="${_repo_env_prev_MASTERLIMITER_RELEASE_BUILD_DIR}"; unset _repo_env_prev_MASTERLIMITER_RELEASE_BUILD_DIR; fi
if [[ "${_repo_env_prev_INSTALL_DOMAIN+x}" == "x" ]]; then export INSTALL_DOMAIN="${_repo_env_prev_INSTALL_DOMAIN}"; unset _repo_env_prev_INSTALL_DOMAIN; fi
if [[ "${_repo_env_prev_MASTERLIMITER_AAX_USE_SYSTEM_WIDE_LIBRARY+x}" == "x" ]]; then export MASTERLIMITER_AAX_USE_SYSTEM_WIDE_LIBRARY="${_repo_env_prev_MASTERLIMITER_AAX_USE_SYSTEM_WIDE_LIBRARY}"; unset _repo_env_prev_MASTERLIMITER_AAX_USE_SYSTEM_WIDE_LIBRARY; fi

return 0 2>/dev/null || exit 0
