#!/bin/bash
set -euo pipefail

# 1. Setup: Define container properties
CONTAINER_NAME="pg_test_$(openssl rand -hex 6)"
# Pick a random high port to avoid collisions during parallel test execution
DB_PORT=$(python3 -c 'import socket; s=socket.socket(); s.bind(("", 0)); print(s.getsockname()[1]); s.close()')

# 2. Cleanup: Ensure container is killed on exit/failure
cleanup() {
  echo "Stopping container "$CONTAINER_NAME"..."
  docker stop "$CONTAINER_NAME" >/dev/null 2>&1 || true
  docker rm "$CONTAINER_NAME" >/dev/null 2>&1 || true
}
trap cleanup EXIT

# 3. Start Postgres Container
echo "Starting Postgres on port "$DB_PORT"..."
docker run --rm -d \
  --name "$CONTAINER_NAME" \
  -e POSTGRES_USER=postgres \
  -e POSTGRES_PASSWORD=password \
  -e POSTGRES_DB=howling \
  -p "127.0.0.1:$DB_PORT:5432" \
  postgres:16-alpine > /dev/null

# 4. Wait for Health (pg_isready)
echo "Waiting for DB to accept connections..."
MAX_RETRIES=20
COUNT=0
until docker exec "$CONTAINER_NAME" pg_isready -U postgres > /dev/null 2>&1; do
  sleep 0.5
  COUNT=$((COUNT+1))
  if [ $COUNT -ge $MAX_RETRIES ]; then
    echo "Timeout waiting for Postgres to start."
    docker logs "$CONTAINER_NAME"
    exit 1
  fi
done
sleep 1 # Give the database a moment more to fully initialize.

# 5. Locate and Run the C++ Test Binary
TEST_BINARY="$1" # Get the test binary path from the first argument

if [ ! -f "$TEST_BINARY" ]; then
    echo "Error: Could not find test binary at $TEST_BINARY"
    exit 1
fi

echo "Running C++ Integration Tests..."
"$TEST_BINARY" \
  --pg_host=127.0.0.1 \
  --pg_port="$DB_PORT" \
  --pg_user=postgres \
  --pg_password=password \
  --pg_database=howling
