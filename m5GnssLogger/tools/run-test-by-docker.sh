#!/bin/bash

# Script to run PlatformIO unit tests inside a Docker container
# Usage: ./tools/run-test-by-docker.sh

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Get the script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
DOCKERFILE="$SCRIPT_DIR/Dockerfile.test"
IMAGE_NAME="m5-gnss-logger-test"
CONTAINER_NAME="m5-gnss-logger-test-runner"

echo -e "${YELLOW}Building Docker image for testing...${NC}"
docker build -t "$IMAGE_NAME" -f "$DOCKERFILE" "$REPO_ROOT"

echo -e "${YELLOW}Running tests...${NC}"
docker run --rm \
    -v "$REPO_ROOT:/workspace" \
    -w /workspace/m5GnssLogger \
    --name "$CONTAINER_NAME" \
    "$IMAGE_NAME" \
    pio test -e native

echo -e "${GREEN}✓ All tests passed!${NC}"
