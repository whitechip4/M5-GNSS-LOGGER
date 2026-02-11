export interface Env {
  BUCKET: R2Bucket;
}

interface GNSSPoint {
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

export default {
  /**
   * Handle HTTP requests
   */
  async fetch(request: Request, env: Env, ctx: ExecutionContext): Promise<Response> {
    console.log('Worker triggered');
    
    // For testing via HTTP
    const url = new URL(request.url);
    if (url.pathname === '/test') {
      return new Response(JSON.stringify({ status: 'Worker is running' }), {
        headers: { 'Content-Type': 'application/json' }
      });
    }
    
    return new Response('GPX Converter Worker - Use R2 events to trigger');
  },

  /**
   * Scheduled event handler (for periodic tasks if needed)
   */
  async scheduled(event: ScheduledEvent, env: Env, ctx: ExecutionContext): Promise<void> {
    console.log('Scheduled event:', event.cron);
  },

  /**
   * R2 object notification handler
   * This is triggered when objects are added/modified in R2
   */
  async r2Objects(
    event: R2ObjectsEvent,
    env: Env,
    ctx: ExecutionContext
  ): Promise<void> {
    console.log('R2 objects event triggered');
    console.log('Bucket:', event.bucket);
    console.log('Changes:', event.changes.length);

    for (const change of event.changes) {
      console.log('Processing object:', change.key);

      // Only process CSV files in gnss-data/ directory
      if (!change.key.startsWith('gnss-data/') || !change.key.endsWith('.csv')) {
        console.log('Skipping non-csv file or wrong directory:', change.key);
        continue;
      }

      // Skip already processed GPX files
      if (change.key.includes('/gpx/')) {
        console.log('Skipping GPX file:', change.key);
        continue;
      }

      try {
        // Get object from R2
        const object = await env.BUCKET.get(change.key);
        if (!object) {
          console.error('Object not found:', change.key);
          continue;
        }

        // Read CSV content
        const csvText = await object.text();
        console.log('CSV content length:', csvText.length);

        // Parse CSV
        const points = parseCSV(csvText);
        
        if (points.length === 0) {
          console.log('No valid points found in CSV:', change.key);
          continue;
        }

        // Generate output path: gnss-data/YYYYMMDD/gpx/filename.gpx
        const baseOutputPath = generateGPXPath(change.key);

        // Convert and upload GPX (may split into multiple files if too large)
        await convertCSVToGPXAndUpload(points, baseOutputPath, env.BUCKET, change.key);

      } catch (error) {
        console.error('Error processing object:', change.key, error);
      }
    }
  }
};

/**
 * Convert CSV points to GPX format and upload to R2
 * Handles file size limitation and splitting
 * @param points - Array of GNSS points
 * @param basePath - Base output path for GPX files
 * @param bucket - R2 bucket instance
 * @param sourceFileName - Source file name for metadata
 */
async function convertCSVToGPXAndUpload(
  points: GNSSPoint[],
  basePath: string,
  bucket: R2Bucket,
  sourceFileName: string
): Promise<void> {
  let fileNumber = 0;
  let currentStartIndex = 0;

  while (currentStartIndex < points.length) {
    const outputPath = fileNumber === 0 ? basePath : getSplitFilePath(basePath, fileNumber);
    const gpxContent = generateGPX(points, currentStartIndex, sourceFileName, fileNumber);
    
    // Check file size and split if needed
    const fileSize = new Blob([gpxContent]).size;
    
    if (fileSize > MAX_GPX_FILE_SIZE) {
      // Find how many points fit
      let endOfSegment = currentStartIndex + 1;
      while (endOfSegment <= points.length) {
        const testSegment = points.slice(currentStartIndex, endOfSegment);
        const testGPX = generateGPX(testSegment, 0, sourceFileName, fileNumber);
        const testSize = new Blob([testGPX]).size;
        
        if (testSize > MAX_GPX_FILE_SIZE) {
          break;
        }
        endOfSegment++;
      }
      
      // Upload the segment that fits
      const segmentPoints = points.slice(currentStartIndex, endOfSegment - 1);
      const segmentGPX = generateGPX(segmentPoints, 0, sourceFileName, fileNumber);
      await uploadGPXToR2(bucket, outputPath, segmentGPX);
      
      console.log(`Uploaded segment ${fileNumber + 1}: ${outputPath}, ${segmentPoints.length} points`);
      
      // Move to next segment
      currentStartIndex = endOfSegment - 1;
      fileNumber++;
    } else {
      // Upload complete file
      await uploadGPXToR2(bucket, outputPath, gpxContent);
      console.log(`Uploaded: ${outputPath}, ${points.length - currentStartIndex} points, ${fileSize} bytes`);
      break;
    }
  }
}

/**
 * Upload GPX content to R2
 * @param bucket - R2 bucket instance
 * @param path - Output file path
 * @param gpxContent - GPX content string
 */
async function uploadGPXToR2(bucket: R2Bucket, path: string, gpxContent: string): Promise<void> {
  await bucket.put(path, gpxContent, {
    httpMetadata: {
      contentType: 'application/gpx+xml'
    }
  });
}

/**
 * Generate GPX file path for split files
 * @param basePath - Original base path
 * @param fileNumber - Split file number
 * @returns Path for split file
 */
function getSplitFilePath(basePath: string, fileNumber: number): string {
  // Replace .gpx with _N.gpx
  return basePath.replace('.gpx', `_${fileNumber}.gpx`);
}

/**
 * Generate GPX content from points
 * @param points - Array of GNSS points
 * @param startIndex - Starting index (0 for new files)
 * @param sourceFileName - Source file name for metadata
 * @param fileNumber - File number for split files
 * @returns GPX formatted string
 */
function generateGPX(
  points: GNSSPoint[],
  startIndex: number,
  sourceFileName: string,
  fileNumber: number
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
  const startTime = formatDateTimeForGPX(firstPoint.date, firstPoint.time);

  // Generate track points
  const trackPoints = points.map(p => 
    `      <trkpt lat="${p.lat.toFixed(7)}" lon="${p.lng.toFixed(7)}">
        <ele>${p.alt.toFixed(1)}</ele>
        <time>${formatDateTimeForGPX(p.date, p.time)}</time>
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
 * Format date and time for GPX (ISO 8601 with Z suffix)
 * @param date - Date string (YYYY/MM/DD)
 * @param time - Time string (HH:MM:SS)
 * @returns Formatted datetime string
 */
function formatDateTimeForGPX(date: string, time: string): string {
  // Convert YYYY/MM/DD to YYYY-MM-DD
  const isoDate = date.replace(/\//g, '-');
  return `${isoDate}T${time}Z`;
}

/**
 * Parse CSV content into array of GNSS points
 * @param csvText - CSV file content
 * @returns Array of GNSSPoint objects
 */
function parseCSV(csvText: string): GNSSPoint[] {
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
 * @param csvPath - Original CSV file path
 * @returns GPX file path
 */
function generateGPXPath(csvPath: string): string {
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