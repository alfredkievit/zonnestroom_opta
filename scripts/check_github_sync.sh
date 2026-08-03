#!/usr/bin/env bash
set -euo pipefail

if ! git rev-parse --is-inside-work-tree >/dev/null 2>&1; then
  echo "ERROR: run this script inside a git repository."
  exit 1
fi

REMOTE_NAME="origin"
BRANCH_NAME="$(git rev-parse --abbrev-ref HEAD)"
REMOTE_REF="${REMOTE_NAME}/${BRANCH_NAME}"

CRITICAL_PATHS=(
  "opta2_hottub/src/comm_manager.cpp"
  "opta2_hottub/src/comm_manager.h"
  "opta2_hottub/src/main.cpp"
  "opta2_hottub/src/config.h"
  "opta2_hottub/src/hottub_logic.cpp"
  "opta2_hottub/src/irrigation_logic.cpp"
  "opta2_hottub/src/ha_interface.cpp"
  "opta2_hottub/src/types.h"
)

echo "=== GitHub Sync Check ==="
echo "Branch: ${BRANCH_NAME}"

echo
echo "[1/5] Fetch latest remote refs"
git fetch "${REMOTE_NAME}" --prune

if ! git show-ref --quiet --verify "refs/remotes/${REMOTE_REF}"; then
  echo "ERROR: remote ref ${REMOTE_REF} does not exist."
  exit 2
fi

LOCAL_HEAD="$(git rev-parse HEAD)"
REMOTE_HEAD="$(git rev-parse "${REMOTE_REF}")"
read -r BEHIND AHEAD < <(git rev-list --left-right --count "${REMOTE_REF}...HEAD")

echo
echo "[2/5] Local vs Cloud"
echo "Local HEAD : ${LOCAL_HEAD}"
echo "Cloud HEAD : ${REMOTE_HEAD}"
echo "Behind/Ahead (local): ${BEHIND}/${AHEAD}"

if [[ "${BEHIND}" != "0" ]]; then
  echo "WARNING: local branch is behind cloud by ${BEHIND} commit(s)."
fi
if [[ "${AHEAD}" != "0" ]]; then
  echo "INFO: local branch is ahead of cloud by ${AHEAD} commit(s)."
fi

echo
echo "[3/5] Local working tree changes"
if [[ -n "$(git status --porcelain)" ]]; then
  git status --short
else
  echo "Working tree clean."
fi

echo
echo "[4/5] Critical Opta2 file delta vs cloud"
CRITICAL_DIFF=0
for p in "${CRITICAL_PATHS[@]}"; do
  if ! git cat-file -e "HEAD:${p}" >/dev/null 2>&1; then
    continue
  fi
  if ! git diff --quiet "${REMOTE_REF}" -- "${p}"; then
    echo "- changed: ${p}"
    CRITICAL_DIFF=1
  fi
done
if [[ "${CRITICAL_DIFF}" == "0" ]]; then
  echo "No critical file differences vs cloud."
fi

echo
echo "[5/5] Recent cloud commits"
git --no-pager log --oneline -n 5 "${REMOTE_REF}"

echo
echo "Result summary:"
if [[ "${BEHIND}" == "0" && "${AHEAD}" == "0" ]]; then
  echo "- local and cloud are in sync"
else
  echo "- local and cloud are NOT in sync"
fi
if [[ -n "$(git status --porcelain)" ]]; then
  echo "- working tree has uncommitted changes"
else
  echo "- working tree is clean"
fi
