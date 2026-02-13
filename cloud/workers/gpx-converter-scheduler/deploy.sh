#!/bin/bash
# Deploy script for Cloudflare Worker
# This script sources .dev.vars for sensitive values and temporarily
# substitutes environment variables in wrangler.toml before deployment.

set -e
set +H  # Disable history expansion to avoid issues with special characters

# Change to script directory
cd "$(dirname "$0")"

# Load environment variables from ../../.config/.dev.vars (common config)
set -a
source ../../.config/.dev.vars
set +a

# Check required variables
if [ -z "$R2_BUCKET_NAME" ]; then
  echo "Error: R2_BUCKET_NAME not set in ../../.config/.dev.vars"
  exit 1
fi

if [ -z "$CLOUDFLARE_ACCOUNT_ID" ]; then
  echo "Error: CLOUDFLARE_ACCOUNT_ID not set in ../../.config/.dev.vars"
  exit 1
fi

if [ -z "$CLOUDFLARE_API_TOKEN" ]; then
  echo "Error: CLOUDFLARE_API_TOKEN not set in ../../.config/.dev.vars"
  echo ""
  echo "To create an API Token:"
  echo "  1. Go to https://dash.cloudflare.com/profile/api-tokens"
  echo "  2. Click 'Create Token'"
  echo "  3. Use the 'Edit Cloudflare Workers' template or create custom with:"
  echo "     - Account > Workers Scripts > Edit"
  echo "     - Account > Cloudflare R2 > Edit"
  echo "  4. Add the token to .dev.vars as CLOUDFLARE_API_TOKEN=..."
  exit 1
fi

# Create backup of wrangler.toml
cp wrangler.toml wrangler.toml.bak

# Substitute environment variables in wrangler.toml
sed -i "s|\${R2_BUCKET_NAME}|${R2_BUCKET_NAME}|g" wrangler.toml
sed -i "s|\${PAGES_FUNCTION_URL}|${PAGES_FUNCTION_URL}|g" wrangler.toml

# Export API token for wrangler
export CLOUDFLARE_API_TOKEN

# Deploy (capture output to extract worker URL)
npm install
DEPLOY_OUTPUT=$(npm run deploy 2>&1 | tr -d '\r')
echo "$DEPLOY_OUTPUT"

# Restore original wrangler.toml with environment variable placeholders
mv wrangler.toml.bak wrangler.toml

# After successful deployment, generate worker URL file
# Extract the worker URL from wrangler output (format: https://worker-name.subdomain.workers.dev)
WORKER_NAME=$(grep '^name = ' wrangler.toml | sed 's/name = "\(.*\)"/\1/' | tr -d ' \n\r')
# Extract URL using awk (most reliable method)
WORKER_URL=$(echo "$DEPLOY_OUTPUT" | awk -v name="$WORKER_NAME" '
BEGIN {
  url_pattern = "^https://"
  worker_pattern = "workers\\.dev$"
}
{
  for (i = 1; i <= NF; i++) {
    if ($i ~ url_pattern && $i ~ name && $i ~ worker_pattern) {
      gsub(/[^a-zA-Z0-9:\/.=-]/, "", $i)
      print $i
      exit
    }
  }
}
')

if [ -z "$WORKER_URL" ]; then
  echo "Debug: Deploy output saved to /tmp/deploy-debug.txt" >&2
  echo "$DEPLOY_OUTPUT" > /tmp/deploy-debug.txt
  echo "Warning: Could not extract worker URL from deploy output" >&2
  echo "WORKER_NAME=$WORKER_NAME" >&2
  exit 1
fi

URL_FILE="tools/.worker-url"

echo "$WORKER_URL" > "$URL_FILE"
echo ""
echo "Worker URL: $WORKER_URL"
echo "URL saved to: $URL_FILE"
echo ""
echo "You can now run the manual trigger script:"
echo "  ./tools/manual_trigger.sh"
echo ""
echo "Deployment complete!"
