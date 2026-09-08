#!/bin/bash
set -euo pipefail

# Change to workspace infrastructure directory.
if [ -n "${BUILD_WORKSPACE_DIRECTORY:-}" ]; then
  cd "${BUILD_WORKSPACE_DIRECTORY}/infrastructure"
else
  SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
  cd "${SCRIPT_DIR}"
fi

# Recreate the executor container with the newly loaded image.
exec docker compose -f docker-compose.yml restart "$@"
