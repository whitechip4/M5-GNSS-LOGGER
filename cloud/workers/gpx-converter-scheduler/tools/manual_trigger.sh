#!/bin/bash

# Manual Trigger Script for GPX Converter Scheduler Worker
# This script allows manual triggering of the GPX conversion process
# for testing or on-demand processing.

set -e

# Change to script directory
cd "$(dirname "$0")/.."

# Default values
# Note: Don't override SCHEDULER_WORKER_URL if already set in environment
: "${SCHEDULER_WORKER_URL:=}"
HEALTH_ONLY=false
VERBOSE=false

# Function to display usage
usage() {
  cat << EOF
Usage: $(basename "$0") [OPTIONS] [WORKER_URL]

Trigger the GPX converter scheduler worker manually.

Arguments:
  WORKER_URL           Optional URL of the scheduler worker
                       (default: SCHEDULER_WORKER_URL from .dev.vars or placeholder)

Options:
  --health             Only perform health check, don't trigger
  --verbose            Enable verbose output with curl details
  -h, --help           Display this help message

Environment Variables:
  SCHEDULER_WORKER_URL  Read from .dev.vars if available

Examples:
  # Trigger with default URL from .dev.vars
  $(basename "$0")

  # Trigger with custom URL
  $(basename "$0") https://gpx-converter-scheduler.myaccount.workers.dev

  # Health check only
  $(basename "$0") --health

  # Verbose mode
  $(basename "$0") --verbose

EOF
  exit 0
}

# Parse arguments
while [[ $# -gt 0 ]]; do
  case $1 in
    --health)
      HEALTH_ONLY=true
      shift
      ;;
    --verbose)
      VERBOSE=true
      shift
      ;;
    -h|--help)
      usage
      ;;
    -*)
      echo "Error: Unknown option: $1" >&2
      usage
      ;;
    *)
      SCHEDULER_WORKER_URL="$1"
      shift
      ;;
  esac
done

# Load URL from auto-generated file, .dev.vars, or environment
if [ -z "$SCHEDULER_WORKER_URL" ]; then
  # Check for auto-generated worker URL file first
  if [ -f "tools/.worker-url" ]; then
    SCHEDULER_WORKER_URL=$(cat tools/.worker-url)
  # Fall back to .dev.vars
  elif [ -f .dev.vars ]; then
    # Source the .dev.vars file to get environment variables
    set -a
    source .dev.vars
    set +a
  fi
  # After sourcing, check if variable was set
  if [ -n "$SCHEDULER_WORKER_URL" ]; then
    : # URL loaded from file or .dev.vars
  fi
fi

# Fallback to placeholder
if [ -z "$SCHEDULER_WORKER_URL" ]; then
  SCHEDULER_WORKER_URL="https://gpx-converter-scheduler.YOUR_ACCOUNT.workers.dev"
fi

# Remove trailing slash
SCHEDULER_WORKER_URL="${SCHEDULER_WORKER_URL%/}"

echo "=== Scheduler Worker Manual Trigger ==="
echo "Worker URL: $SCHEDULER_WORKER_URL"
echo ""

# Build curl options
CURL_OPTS="-s"
if [ "$VERBOSE" = true ]; then
  CURL_OPTS="-v"
fi

# Check if jq is available
HAS_JQ=false
if command -v jq &> /dev/null; then
  HAS_JQ=true
fi

# Format JSON output
format_json() {
  if [ "$HAS_JQ" = true ]; then
    jq .
  else
    cat
  fi
}

# Health check
echo "Health check:"
HEALTH_RESPONSE=$(curl $CURL_OPTS "${SCHEDULER_WORKER_URL}/test")
echo "$HEALTH_RESPONSE" | format_json
echo ""

# Check health status
if [ "$HAS_JQ" = true ]; then
  HEALTH_STATUS=$(echo "$HEALTH_RESPONSE" | jq -r '.status // empty')
  if [ -z "$HEALTH_STATUS" ]; then
    echo "Error: Health check failed - invalid response" >&2
    exit 1
  fi
  echo "✓ Worker is running"
  echo ""
fi

# Exit if health check only
if [ "$HEALTH_ONLY" = true ]; then
  echo "=== Health check complete ==="
  exit 0
fi

# Trigger conversion
echo "Triggering conversion..."
TRIGGER_RESPONSE=$(curl $CURL_OPTS -X POST "${SCHEDULER_WORKER_URL}/trigger" \
  -H "Content-Type: application/json")
echo "$TRIGGER_RESPONSE" | format_json
echo ""

# Check trigger result
if [ "$HAS_JQ" = true ]; then
  STATUS=$(echo "$TRIGGER_RESPONSE" | jq -r '.status // empty')
  if [ "$STATUS" = "Triggered successfully" ]; then
    PROCESSED=$(echo "$TRIGGER_RESPONSE" | jq -r '.result.processed // 0')
    SKIPPED=$(echo "$TRIGGER_RESPONSE" | jq -r '.result.skipped // 0')
    TOTAL=$(echo "$TRIGGER_RESPONSE" | jq -r '.result.total // 0')
    echo "✓ Trigger completed: Processed $PROCESSED, Skipped $SKIPPED, Total $TOTAL"
  elif [ "$STATUS" = "Error" ]; then
    echo "✗ Trigger failed" >&2
    exit 1
  fi
fi

echo "=== Trigger complete ==="
