#!/bin/bash
set -euo pipefail

# Change to workspace infrastructure directory.
if [ -n "${BUILD_WORKSPACE_DIRECTORY:-}" ]; then
  cd "${BUILD_WORKSPACE_DIRECTORY}/infrastructure"
else
  SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
  cd "${SCRIPT_DIR}"
fi

# Execute docker compose up, forwarding any additional command-line arguments.
exec docker compose -f docker-compose.yml up "$@"
