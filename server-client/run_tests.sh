#!/bin/bash

set -e

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
SERVER_DIR="$SCRIPT_DIR"
DATABASE_DIR="$SCRIPT_DIR/../database"

echo "=== Building Database ==="
cd "$DATABASE_DIR"
make clean
make build

echo ""
echo "=== Building Server ==="
cd "$SERVER_DIR"
make clean
make build

echo ""
echo "=== Starting Server ==="
set -a
source "$DATABASE_DIR/.env"
set +a

./messenger_server &
SERVER_PID=$!
echo "Server started with PID $SERVER_PID"

sleep 2

echo ""
echo "=== Running Client Tests ==="
./test_client

echo ""
echo "=== Cleaning up ==="
kill $SERVER_PID 2>/dev/null || true
wait $SERVER_PID 2>/dev/null || true

echo "=== Test completed ==="
