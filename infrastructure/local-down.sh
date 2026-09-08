#!/bin/bash
set -euo pipefail

# If executed via 'bazel run', change to workspace infrastructure directory.
if [ -n "${BUILD_WORKSPACE_DIRECTORY:-}" ]; then
  cd "${BUILD_WORKSPACE_DIRECTORY}/infrastructure"
else
  SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
  cd "${SCRIPT_DIR}"
fi

docker compose -f docker-compose.yml down "$@"
