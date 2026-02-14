#!/bin/bash
# Deploy script for Cloudflare Pages Functions
# This script sources .env for sensitive values and temporarily
# substitutes environment variables in wrangler.toml before deployment.

set -e

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
  exit 1
fi

# Create backup of wrangler.toml
cp wrangler.toml wrangler.toml.bak

# Substitute environment variables in wrangler.toml
sed -i "s|\${R2_BUCKET_NAME}|${R2_BUCKET_NAME}|g" wrangler.toml

# Deploy
echo "Deploying to Cloudflare Pages..."
npx wrangler pages deploy --project-name=gpx-converter

# Restore original wrangler.toml with environment variable placeholders
mv wrangler.toml.bak wrangler.toml

echo "Deployment complete!"
echo "Pages Function URL: https://gpx-converter.<your-account>.workers.dev/gpx-converter"
