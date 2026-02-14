# M5-GNSS-LOGGER Cloudflare Deployment

## Overview

M5-GNSS-LOGGER uses a hybrid Cloudflare deployment to convert CSV files uploaded to R2 into GPX format for Google My Maps.

**Architecture:**
- **Scheduler Worker**: Triggers every hour via cron, calls Pages Functions via HTTP
- **Pages Functions**: Main GPX conversion logic with Node.js runtime (handles 10MB+ files)
- **R2 Storage**: `<your bucket name>` bucket stores CSV and GPX files

## Deployment Environments

### Production
Deployed to production branch with:
- Scheduler Worker: `https://gpx-converter-scheduler.<your-account>.workers.dev`
- Pages Functions: `https://<your-project-name>.gpx-converter.pages.dev/gpx-converter`

### Preview
For testing changes before production:
```bash
cd cloud/pages-functions/gpx-converter
npx wrangler pages deploy . --project-name=gpx-converter
```

Preview URL: `https://<deploy-branch>.<your-project-name>.pages.dev/gpx-converter`

## Usage

### Automatic Processing (Cron)

The Scheduler Worker runs every hour (at minute 0) and automatically:
1. Scans `gnss-data/` directory in R2 for CSV files
2. Converts CSV files to GPX format
3. Uploads GPX files to `gnss-data/YYYYMMDD/gpx/` directory

### Manual Processing

Trigger GPX conversion anytime via HTTP request:

```bash
# Process all CSV files
curl https://gpx-converter.<your-account>.workers.dev/gpx-converter
```

Response example:
```json
{
  "status": "Processing complete",
  "processed": 5,
  "skipped": 127,
  "total": 132
}
```

## Configuration

Configuration is stored in `.dev.vars` files (gitignored):

```bash
# Scheduler Worker
cloud/workers/gpx-converter/.dev.vars

# Pages Functions
cloud/pages-functions/gpx-converter/.dev.vars
```

Required environment variables:
- `CLOUDFLARE_ACCOUNT_ID` - Cloudflare account ID
- `CLOUDFLARE_API_TOKEN` - API token with Workers Scripts (Edit) + R2 (Edit) permissions
- `R2_BUCKET_NAME` - R2 bucket name (e.g., `gnss-data`)
- `PAGES_FUNCTION_URL` - URL to Pages Functions endpoint (e.g., `https://<project-name>.gpx-converter.pages.dev/gpx-converter`)

## Deployment

### Initial Deployment

```bash
# Deploy Scheduler Worker
cd cloud/workers/gpx-converter
./deploy.sh

# Deploy Pages Functions
cd cloud/pages-functions/gpx-converter
./deploy.sh
```

## Data Flow

```
M5Stack → Upload CSV to R2 → Scheduler Worker (cron) → Pages Functions → Convert CSV → Upload GPX to R2
                                           ↑
                                      Manual HTTP Request (optional)
```

## File Organization

### Input
- CSV files uploaded to: `gnss-data/` directory
- Example: `gnss-data/gnss_csv_data_20240101_120000.csv`

### Output
- GPX files saved to: `gnss-data/YYYYMMDD/gpx/` directory
- Example: `gnss-data/20240101/gpx/gnss_csv_data_20240101_120000.gpx`

## CSV Format

Expected CSV format (header row required):

```
date,time,lat,lng,alt,spd,siv,hdop
2024/01/01,12:00:00,35.6895,139.6917,50.0,5.5,8,1.2
```

## Features

- ✅ **Large File Support**: Processes 10MB+ CSV files (Pages Functions Node.js runtime)
- ✅ **Automatic Splitting**: Splits GPX files >4MB for Google My Maps compatibility
- ✅ **Cron Trigger**: Runs every hour automatically
- ✅ **Manual Trigger**: Process anytime via HTTP request
- ✅ **Incremental Processing**: Skips already converted files
- ✅ **Free Tier**: Runs on Cloudflare free tier

## Troubleshooting

### Scheduler Worker Not Triggering

Check cron schedule:
```bash
npx wrangler schedules list
```

### Pages Functions Not Processing

Check logs:
```bash
# Scheduler Worker logs
cd cloud/workers/gpx-converter-scheduler
npm run tail

# Pages Functions logs
cd cloud/pages-functions/gpx-converter
npx wrangler pages tail
```

## Development

### Local Testing (Scheduler Worker)

```bash
cd cloud/workers/gpx-converter
npx wrangler dev
```

### Local Testing (Pages Functions)

```bash
cd cloud/pages-functions/gpx-converter
npx wrangler pages dev
```

## Architecture Notes

### Why Pages Functions + Scheduler Worker?

Cloudflare **Workers** has CPU time limits (10ms/day free tier) unsuitable for large CSV processing.

Cloudflare **Pages Functions** with Node.js runtime allows longer processing time (minutes).

However, Pages Functions doesn't support cron triggers natively.

**Solution**: Hybrid architecture
- Lightweight Scheduler Worker with cron trigger
- Calls Pages Functions via HTTP when triggered
- Pages Functions handles the heavy CSV→GPX processing

This provides:
✅ Automatic processing (via cron)
✅ Large file support (via Node.js runtime)
✅ Free tier operation
