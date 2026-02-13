// Shared utilities for GNSS/GPX processing
// Used by Pages Functions and potentially by Workers

export interface Env {
  BUCKET: R2Bucket;
}

export interface GNSSPoint {
  date: string;
  time: string;
  lat: number;
  lng: number;
  alt: number;
  spd: number;
  siv: number;
  hdop: number;
}

// Configuration constants
const DEFAULT_AUTHOR_NAME = "M5-GNSS-LOGGER";
const DEFAULT_TITLE = "GNSS Data";
const MAX_GPX_FILE_SIZE = 4 * 1024 * 1024; // 4MB (Google My Maps limit is 5MB)

/**
 * Process a single CSV file and convert to GPX
 */
export async function processCSVFile(key: string, env: Env): Promise<boolean> {
  console.log('Processing object:', key);

  // Only process CSV files in gnss-data/ directory
  if (!key.startsWith('gnss-data/') || !key.endsWith('.csv')) {
    console.log('Skipping non-csv file or wrong directory:', key);
    return false;
  }

  // Skip already processed GPX files
  if (key.includes('/gpx/')) {
    console.log('Skipping GPX file:', key);
    return false;
  }

  // Check if GPX already exists
  const gpxPath = generateGPXPath(key);
  const existingGpx = await env.BUCKET.get(gpxPath);
  if (existingGpx) {
    console.log('GPX already exists, skipping:', gpxPath);
    return false;
  }

  try {
    // Get object from R2
    const object = await env.BUCKET.get(key);
    if (!object) {
      console.error('Object not found:', key);
      return false;
    }

    // Read CSV content
    const csvText = await object.text();

    // Get timezone offset from metadata (default: +9 for JST)
    const timezoneMeta = object.customMetadata?.timezone;
    let timezoneOffset = timezoneMeta ? parseInt(timezoneMeta, 10) : 9;

    // Validate timezone offset (NaN check)
    if (isNaN(timezoneOffset)) {
      console.warn('Invalid timezone metadata, using default (JST+9):', timezoneMeta);
      timezoneOffset = 9;
    }
    console.log(`Timezone offset from metadata: ${timezoneOffset} (metadata: ${timezoneMeta || 'not found, using default'})`);

    // Parse CSV
    const points = parseCSV(csvText);

    if (points.length === 0) {
      return false;
    }

    // Convert and upload GPX (may split into multiple files if too large)
    await convertCSVToGPXAndUpload(points, gpxPath, env.BUCKET, key, timezoneOffset);
    return true;

  } catch (error) {
    console.error('Error processing object:', key, error);
    return false;
  }
}

/**
 * Convert CSV points to GPX format and upload to R2
 * Handles file size limitation and splitting
 */
export async function convertCSVToGPXAndUpload(
  points: GNSSPoint[],
  basePath: string,
  bucket: R2Bucket,
  sourceFileName: string,
  timezoneOffset: number
): Promise<void> {
  let fileNumber = 0;
  let currentStartIndex = 0;

  while (currentStartIndex < points.length) {
    const outputPath = fileNumber === 0 ? basePath : getSplitFilePath(basePath, fileNumber);
    const gpxContent = generateGPX(points, currentStartIndex, sourceFileName, fileNumber, timezoneOffset);

    // Check file size and split if needed
    const fileSize = new Blob([gpxContent]).size;

    if (fileSize > MAX_GPX_FILE_SIZE) {
      // Binary search to find how many points fit
      let low = currentStartIndex + 1;
      let high = points.length;

      while (low < high) {
        const mid = Math.floor((low + high) / 2);
        const testSegment = points.slice(currentStartIndex, mid);
        const testGPX = generateGPX(testSegment, 0, sourceFileName, fileNumber, timezoneOffset);
        const testSize = new Blob([testGPX]).size;

        if (testSize > MAX_GPX_FILE_SIZE) {
          high = mid;
        } else {
          low = mid + 1;
        }
      }

      // Upload the segment that fits (low - 1 is the largest valid index)
      const segmentPoints = points.slice(currentStartIndex, low - 1);
      const segmentGPX = generateGPX(segmentPoints, 0, sourceFileName, fileNumber, timezoneOffset);
      await uploadGPXToR2(bucket, outputPath, segmentGPX);

      // Move to next segment
      currentStartIndex = low - 1;
      fileNumber++;
    } else {
      // Upload complete file
      await uploadGPXToR2(bucket, outputPath, gpxContent);
      break;
    }
  }
}

/**
 * Upload GPX content to R2 (overwrites if exists)
 */
async function uploadGPXToR2(bucket: R2Bucket, path: string, gpxContent: string): Promise<void> {
  await bucket.put(path, gpxContent, {
    httpMetadata: {
      contentType: 'application/gpx+xml'
    }
  });
  console.log(`✅ Uploaded GPX: ${path}`);
}

/**
 * Generate GPX file path for split files
 */
function getSplitFilePath(basePath: string, fileNumber: number): string {
  // Replace .gpx with _N.gpx
  return basePath.replace('.gpx', `_${fileNumber}.gpx`);
}

/**
 * Generate GPX content from points
 */
export function generateGPX(
  points: GNSSPoint[],
  startIndex: number,
  sourceFileName: string,
  fileNumber: number,
  timezoneOffset: number
): string {
  if (points.length === 0) {
    throw new Error('No points to convert');
  }

  const firstPoint = points[0];
  const lastPoint = points[points.length - 1];

  // Calculate bounds (min/max lat/lng)
  let minLat = firstPoint.lat;
  let maxLat = firstPoint.lat;
  let minLng = firstPoint.lng;
  let maxLng = firstPoint.lng;

  for (const point of points) {
    if (point.lat < minLat) minLat = point.lat;
    if (point.lat > maxLat) maxLat = point.lat;
    if (point.lng < minLng) minLng = point.lng;
    if (point.lng > maxLng) maxLng = point.lng;
  }

  // Format start time for metadata
  const startTime = formatDateTimeForGPX(firstPoint.date, firstPoint.time, timezoneOffset);

  // Generate track points
  const trackPoints = points.map(p =>
    `      <trkpt lat="${p.lat.toFixed(7)}" lon="${p.lng.toFixed(7)}">
        <ele>${p.alt.toFixed(1)}</ele>
        <time>${formatDateTimeForGPX(p.date, p.time, timezoneOffset)}</time>
      </trkpt>`
  ).join('\n');

  // Build GPX XML
  const gpx = `<?xml version="1.0" encoding="utf-8"?>
<gpx xmlns="http://www.topografix.com/GPX/1/1" version="1.0" creator="${DEFAULT_AUTHOR_NAME}">
  <metadata>
    <time>${startTime}</time>
    <bounds minlat="${minLat.toFixed(7)}" maxlat="${maxLat.toFixed(7)}" minlon="${minLng.toFixed(7)}" maxlon="${maxLng.toFixed(7)}"/>
  </metadata>
  <trk>
    <name>${DEFAULT_TITLE}${fileNumber > 0 ? ` (Part ${fileNumber + 1})` : ''}</name>
    <trkseg>
${trackPoints}
    </trkseg>
  </trk>
</gpx>`;

  return gpx;
}

/**
 * Format date and time for GPX (ISO 8601 with Z suffix in UTC)
 * @param date Local date string (YYYY/MM/DD)
 * @param time Local time string (HH:MM:SS)
 * @param timezoneOffset Timezone offset in hours (e.g., +9 for JST)
 * @returns UTC datetime string in ISO 8601 format with Z suffix
 */
function formatDateTimeForGPX(date: string, time: string, timezoneOffset: number): string {
  // Convert YYYY/MM/DD to YYYY-MM-DD
  const isoDate = date.replace(/\//g, '-');

  // Build timezone offset string in ISO 8601 format (+HH:mm or -HH:mm)
  const tzOffsetSign = timezoneOffset >= 0 ? '+' : '-';
  const tzOffsetAbs = Math.abs(timezoneOffset);
  const tzOffsetHours = Math.floor(tzOffsetAbs);
  const tzOffsetMinutes = Math.round((tzOffsetAbs - tzOffsetHours) * 60);
  const tzOffsetStr = `${tzOffsetSign}${String(tzOffsetHours).padStart(2, '0')}:${String(tzOffsetMinutes).padStart(2, '0')}`;

  // Create ISO 8601 string with explicit timezone offset
  const localDateTimeWithTz = `${isoDate}T${time}${tzOffsetStr}`;

  // Parse with timezone - Date constructor will handle ISO 8601 with offset correctly
  const localDate = new Date(localDateTimeWithTz);

  // Format to ISO 8601 with Z suffix (UTC)
  return localDate.toISOString().replace(/\.\d{3}Z$/, 'Z');
}

/**
 * Parse CSV content into array of GNSS points
 */
export function parseCSV(csvText: string): GNSSPoint[] {
  const lines = csvText.split('\n').filter(line => line.trim() !== '');

  if (lines.length === 0) {
    return [];
  }

  // Skip header line if it contains column names
  const startIndex = lines[0].toLowerCase().includes('date') ? 1 : 0;
  const dataLines = lines.slice(startIndex);

  const points: GNSSPoint[] = [];

  for (const line of dataLines) {
    const parts = line.split(',');
    if (parts.length < 8) {
      console.log('Skipping malformed line:', line);
      continue;
    }

    // CSV format: date,time,lat,lng,alt,spd,siv,hdop
    // Example: 2024/01/01,12:00:00,35.6895,139.6917,50.0,5.5,8,1.2
    const lat = parseFloat(parts[2]);
    const lng = parseFloat(parts[3]);

    // Validate coordinates
    if (isNaN(lat) || isNaN(lng)) {
      console.log('Skipping invalid coordinates:', parts[2], parts[3]);
      continue;
    }

    const point: GNSSPoint = {
      date: parts[0].trim(),
      time: parts[1].trim(),
      lat: lat,
      lng: lng,
      alt: parseFloat(parts[4]),
      spd: parseFloat(parts[5]),
      siv: parseInt(parts[6]),
      hdop: parseFloat(parts[7])
    };

    points.push(point);
  }

  console.log(`Parsed ${points.length} valid points from CSV`);
  return points;
}

/**
 * Generate GPX file path from CSV file path
 */
export function generateGPXPath(csvPath: string): string {
  // Input: gnss-data/20240101/gnss_csv_data_20240101_120000.csv
  // Output: gnss-data/20240101/gpx/gnss_csv_data_20240101_120000.gpx

  const parts = csvPath.split('/');
  const fileName = parts[parts.length - 1];
  const fileNameWithoutExt = fileName.replace('.csv', '');

  // Extract date from filename (format: gnss_csv_data_YYYYMMDD_HHMMSS.csv)
  // Or from path (gnss-data/YYYYMMDD/filename.csv)
  let dateStr: string;

  const dateMatch = fileName.match(/gnss_csv_data_(\d{8})_/);
  if (dateMatch) {
    dateStr = dateMatch[1];
  } else if (parts.length >= 2 && parts[0] === 'gnss-data') {
    // Try to get date from directory
    dateStr = parts[1];
  } else {
    // Use current date
    dateStr = new Date().toISOString().slice(0, 10).replace(/-/g, '');
  }

  return `gnss-data/${dateStr}/gpx/${fileNameWithoutExt}.gpx`;
}
