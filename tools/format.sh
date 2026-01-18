#!/bin/bash

# Format all C++ source files in the project using clang-format
# This script is intended to be run inside a Docker container with clang-format installed

set -e

echo "=== Running clang-format on C++ source files ==="

# Dry run mode (check only, don't modify files)
DRY_RUN=${DRY_RUN:-false}

if [ "$DRY_RUN" = "true" ]; then
  echo "Running in dry-run mode (check only)"
fi

# Find C++ source files in whitelisted directories only
CPP_FILES=$(find . -type f \( -name "*.cpp" -o -name "*.h" -o -name "*.hpp" \) \
  \( -path "*/m5GnssLogger/src/*" -o \
     -path "*/m5GnssLogger/include/*" -o \
     -path "*/m5GnssLogger/test/*" \))

if [ -z "$CPP_FILES" ]; then
  echo "No C++ source files found."
  exit 0
fi

echo "Found $(echo "$CPP_FILES" | wc -l) C++ source file(s):"
echo "$CPP_FILES" | sed 's/^/  /'

# Check if clang-format is available
if ! command -v clang-format &> /dev/null; then
  echo "Error: clang-format not found. Please install clang-format."
  exit 1
fi

# Run clang-format on each file
ERRORS=0
FORMATTING_NEEDED=false
for file in $CPP_FILES; do
  if [ "$DRY_RUN" = "true" ]; then
    echo "Checking $file..."
    if clang-format -style=file "$file" | diff -u "$file" - > /dev/null; then
      echo "  ✓ Already properly formatted"
    else
      echo "  ✗ Needs formatting"
      FORMATTING_NEEDED=true
    fi
  else
    echo "Formatting $file..."
    if clang-format -i -style=file "$file"; then
      echo "  ✓ Formatted successfully"
    else
      echo "  ✗ Error formatting $file"
      ERRORS=$((ERRORS + 1))
    fi
  fi
done

if [ "$DRY_RUN" = "true" ]; then
  if [ "$FORMATTING_NEEDED" = "false" ]; then
    echo "=== All files are properly formatted ==="
    exit 0
  else
    echo "=== Some files need formatting ==="
    exit 1
  fi
else
  if [ $ERRORS -eq 0 ]; then
    echo "=== All files formatted successfully ==="
    exit 0
  else
    echo "=== Formatting completed with $ERRORS error(s) ==="
    exit 1
  fi
fi
