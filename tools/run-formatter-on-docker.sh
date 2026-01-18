#!/bin/bash

# Script to run the formatter using Docker locally
# This script builds the Docker image if needed and runs the formatter

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
IMAGE_NAME="clang-format-check"
CONTAINER_NAME="clang-format-check-container"

# Default to dry-run mode for safety
MODE="check"

# Parse command line arguments
while [[ $# -gt 0 ]]; do
  case $1 in
    -h|--help)
      echo "Usage: $0 [OPTION]"
      echo "Options:"
      echo "  -h, --help     Show this help message"
      echo "  -fix, --fix    Apply formatting to files (default: check only, dry-run)"
      exit 0
      ;;
    --fix|-fix)
      MODE="apply"
      shift
      ;;
    *)
      echo "Unknown option: $1"
      echo "Use '$0 --help' for usage information."
      exit 1
      ;;
  esac
done

cd "$PROJECT_ROOT"

echo "=== Running formatter using Docker ==="

# Check if Docker is available
if ! command -v docker &> /dev/null; then
  echo "Error: Docker is not installed or not in PATH."
  exit 1
fi

# Build the Docker image if it doesn't exist
if ! docker image inspect "$IMAGE_NAME" &> /dev/null; then
  echo "Building Docker image '$IMAGE_NAME'..."
  docker build -t "$IMAGE_NAME" ./tools
else
  echo "Using existing Docker image '$IMAGE_NAME'."
fi

# Determine the mode
if [ "$MODE" = "apply" ]; then
  echo "Mode: Apply formatting (will modify files)"
  DOCKER_ENV=""
else
  echo "Mode: Check formatting only (dry-run)"
  DOCKER_ENV="-e DRY_RUN=true"
fi

# Run the Docker container
echo "Running formatter..."
docker run --rm \
  $DOCKER_ENV \
  -v "$PROJECT_ROOT:/workspace" \
  --name "$CONTAINER_NAME" \
  "$IMAGE_NAME"

echo "=== Formatter completed ==="
