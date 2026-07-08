#!/bin/bash
# Build script for m5GnssLogger
# Compiles the project without uploading to device

set -e

# Get the script's directory first
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# Find the git repository root from the script's location (works from any execution path)
GIT_ROOT="$(cd "$SCRIPT_DIR" && git rev-parse --show-toplevel)"
# The PlatformIO project is in the m5GnssLogger subdirectory
PROJECT_DIR="$GIT_ROOT/m5GnssLogger"

echo "Building m5GnssLogger..."
echo "Project directory: $PROJECT_DIR"
echo ""

# Use pio from PATH if available, otherwise fall back to the default PlatformIO install location
if command -v pio > /dev/null 2>&1; then
  PIO="pio"
else
  PIO="$HOME/.platformio/penv/bin/pio"
fi

cd "$PROJECT_DIR"
"$PIO" run --environment m5stack-core2

echo ""
echo "Build completed successfully!"
