# GPX Converter - Scheduler Worker

**Note:** The main GPX conversion logic has been migrated to Pages Functions for better handling of large CSV files (10MB+).

This Scheduler Worker triggers the Pages Functions via HTTP every hour.

## Current Deployment

| Component | URL | Purpose |
|-----------|-----|---------|
| **Scheduler Worker** | https://gpx-converter-scheduler.<your-account>.workers.dev | Cron trigger → calls Pages Functions |
| **Pages Functions** | https://gpx-converter.<your-account>.workers.dev/gpx-converter | Main GPX conversion logic |

See [../../README.md](../../README.md) for complete deployment documentation.

## Setup

### Prerequisites

- Node.js 18+
- Cloudflare account with R2 enabled
- Wrangler CLI installed

### Installation

1. Install dependencies:
```bash
npm install
```

2. Configure environment variables:
```bash
cp ../../.config/.dev.vars.example ../../.config/.dev.vars
```

Edit `../../.config/.dev.vars` with your Cloudflare account ID:
```
CLOUDFLARE_ACCOUNT_ID=your_account_id_here
R2_BUCKET_NAME=your_bucket_name_here
CLOUDFLARE_API_TOKEN=your_api_token
```

3. Login to Cloudflare:
```bash
npx wrangler login
```

### Deployment

Deploy to Cloudflare Workers:
```bash
npm run deploy
```

### Local Testing

For local development with `npm run dev`, create a symlink to the shared config:
```bash
ln -s ../../.config/.dev.vars .dev.vars
```

Then test locally (requires R2 bucket to be accessible):
```bash
npm run dev
```

## R2 Configuration

Make sure your R2 bucket is properly configured in `wrangler.toml`:

```toml
[[r2_buckets]]
binding = "BUCKET"
bucket_name = "<your_bucket_name>"
```

## R2 Event Triggers

The worker uses R2 Object Notifications to automatically trigger when new files are added. To enable R2 notifications:

1. Go to Cloudflare Dashboard
2. Navigate to R2 → Your Bucket → Settings
3. Enable "Object Notifications" (if available in your plan)
4. Configure to trigger this worker

Or use the R2 notification API programmatically.

## CSV Format

Expected CSV format (header row required):
```
date,time,lat,lng,alt,spd,siv,hdop
2024/01/01,12:00:00,35.6895,139.6917,50.0,5.5,8,1.2
```

## GPX Format

Output GPX format includes:
- Track points with all metadata
- Elevation data
- Time stamps
- Speed information
- Satellite count
- HDOP accuracy indicator

## File Organization

### Input
- Files uploaded to: `gnss-data/` directory
- Example: `gnss-data/gnss_csv_data_20240101_120000.csv`

### Output
- GPX files saved to: `gnss-data/YYYYMMDD/gpx/` directory
- Example: `gnss-data/20240101/gpx/gnss_csv_data_20240101_120000.gpx`

## Scripts

- `npm run dev` - Start local development server
- `npm run deploy` - Deploy to Cloudflare Workers
- `npm run tail` - View real-time logs

## Troubleshooting

### TypeScript Errors
If you see TypeScript errors about missing types, run:
```bash
npm install
```

### Worker Not Triggering
1. Check that R2 notifications are enabled
2. Verify the worker is deployed
3. Check worker logs: `npm run tail`

### GPX Not Generated
1. Check the CSV file format matches expected format
2. Verify the file is in `gnss-data/` directory
3. Check worker logs for errors