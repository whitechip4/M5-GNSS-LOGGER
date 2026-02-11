# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

M5-GNSS-LOGGER is a GNSS (GPS) logging system with two main components:
1. **M5Stack Core2 Firmware** ([m5GnssLogger/](m5GnssLogger/)) - Captures GPS data and uploads to Cloudflare R2
2. **Cloudflare Worker** ([cloud/workers/gpx-converter/](cloud/workers/gpx-converter/)) - Converts CSV data to GPX format automatically

**Data Flow**: M5Stack captures GNSS data → saves to SD card → uploads to R2 when target WiFi detected → Worker triggers automatically → converts CSV to GPX

## Build and Development Commands

### M5Stack Firmware (PlatformIO)

```bash
cd m5GnssLogger

# Install dependencies
pio pkg install

# Build 
pio run 

# Upload
pio run --target upload --environment m5stack-core2 

# Monitor serial output (for debugging)
pio device monitor
```

### Cloudflare Worker

```bash
cd cloud/workers/gpx-converter

# Install dependencies
npm install

# Local development server
npm run dev

# Deploy to Cloudflare
npm run deploy

# View production logs
npm run tail
```

### Code Formatting (C++)

```bash
# From repository root - check formatting (read-only, for CI)
docker build -t clang-format-check ./tools
docker run --rm -e DRY_RUN=true -v "$(pwd):/workspace" clang-format-check

# Format files (modify in place)
docker run --rm -v "$(pwd):/workspace" clang-format-check
```

Formatting is enforced by GitHub Actions on push/PR to main/master. Uses project-root [`.clang-format`](.clang-format) (Google-based style, 2-space indent, 100 char line limit).

## Architecture

### M5Stack Firmware Modules ([m5GnssLogger/src/](m5GnssLogger/src/))

| Module | Header | Purpose |
|--------|--------|---------|
| GNSS | [gnss.h](m5GnssLogger/include/gnss.h) | GPS data capture via u-blox library |
| Display | [display.h](m5GnssLogger/include/display.h) | Screen rendering (multiple display modes) |
| Storage | [storage.h](m5GnssLogger/include/storage.h) | SD card CSV writing |
| WiFi | (see main.cpp) | WiFi connection for upload |
| R2 | [r2.h](m5GnssLogger/include/r2.h) | Cloudflare R2 upload via AWS Signature V4 |

**Key Types**: [`GNSS_DATA`](m5GnssLogger/include/config.h:10) struct contains all GNSS readings (lat/lng/alt/speed/time/fix quality).

**Environment**: Configuration loaded from `.env` file (gitignored) via pre-build script [`tools/parse_env.py`](tools/parse_env.py) at build time. Template at [`.env.example`](m5GnssLogger/.env.example).

**Thresholds** ([config.h](m5GnssLogger/include/config.h:42)): GNSS data is filtered by HDOP (< 6.0), min satellites (≥ 5), position change (> 0.001°).

### Cloudflare Worker ([cloud/workers/gpx-converter/src/index.ts](cloud/workers/gpx-converter/src/index.ts))

- **Trigger**: R2 bucket notification on object creation
- **Handler**: `r2Objects()` processes new CSV files
- **Features**:
  - Parses CSV format: `date,time,lat,lng,alt,spd,siv,hdop`
  - Generates GPX 1.1 format with metadata
  - Splits large files (>4MB) to respect Google My Maps limits
  - Output path: `gnss-data/YYYYMMDD/gpx/filename.gpx`

**Binding**: R2 bucket bound as `BUCKET` in [wrangler.toml](cloud/workers/gpx-converter/wrangler.toml:6).

## Security

Never commit sensitive files. These are gitignored:
- `m5GnssLogger/.env` - Contains WiFi credentials, R2 keys
- `cloud/workers/gpx-converter/.dev.vars` - Contains Cloudflare credentials

Use template files instead:
- `m5GnssLogger/.env.example`
- `cloud/workers/gpx-converter/.dev.vars.example`

## File Naming Conventions

- **CSV files**: `gnss_csv_data_YYYYMMDD_HHMMSS.csv` (processed data)
- **CSV raw files**: `gnss_csv_data_YYYYMMDD_HHMMSS_raw.csv` (all GNSS messages)
- **GPX files**: Same base name with `.gpx` extension in `gpx/` subfolder

## Device Button Controls

| Button | Function |
|--------|----------|
| Btn A | Toggle display mode (detail/simple) |
| Btn B | Stop recording (with confirmation dialog) |
| Btn C | Cancel stop confirmation |
