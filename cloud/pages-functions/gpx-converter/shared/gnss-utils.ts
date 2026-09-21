// Shared utilities for GNSS/GPX processing
// Used by Pages Functions and potentially by Workers

import { cleanTrack, type TrackPoint, toEpochSec } from "./track-cleaner";
import { buildIndexEntry, type TrackIndexEntry } from "./track-index";

export interface Env {
  BUCKET: R2Bucket;
  CONVERSION_DAYS?: string;
}

export interface GNSSPoint extends TrackPoint {
  date: string;
  time: string;
  lat: number;
  lng: number;
  alt: number;
  spd: number;
  siv: number;
  hdop: number;
  /** u-blox hAcc [m]（新しいファームウェアのCSVのみ） */
  hacc?: number;
  /** 位置飛び補正後の標高 [m]（cleanTrack が設定） */
  ele?: number;
}

/** GPXの<desc>に埋め込むクリーニング統計 */
export interface CleanStats {
  input: number;
  dropped: number;
  altitudeCorrected: number;
}

// Configuration constants
const DEFAULT_AUTHOR_NAME = "M5-GNSS-LOGGER";
/**
 * クリーナー/生成ロジックのバージョン。GPXのcustomMetadataに記録し、
 * force再生成時に同じバージョンで生成済みのGPXはスキップする（CPU時間の節約）。
 * track-cleaner.ts や generateGPX の出力を変えたら上げる。
 */
export const GPX_GENERATOR_VERSION = "5";
/**
 * 記録がこの秒数以上途切れていたら<trkseg>を分ける。
 * 分けないとビューアが途切れ区間を1本の直線で結び、数kmの「飛び」に見える
 */
const SEGMENT_GAP_SEC = 300;
const DEFAULT_TITLE = "GNSS Data";
const MAX_GPX_FILE_SIZE = 4 * 1024 * 1024; // 4MB (Google My Maps limit is 5MB)

/**
 * Process a single CSV file and convert to GPX
 * @param force true の場合は既存GPXがあっても再生成する（クリーナー更新後の再変換用）
 * @returns 変換した場合は出力した各GPX（分割パート毎）の索引エントリ、スキップ/失敗は空配列
 */
export async function processCSVFile(
  key: string,
  env: Env,
  force = false,
  indexedGpxKeys?: Set<string>
): Promise<TrackIndexEntry[]> {
  console.log("Processing object:", key);

  // Only process CSV files in gnss-data/ directory
  if (!key.startsWith("gnss-data/") || !key.endsWith(".csv")) {
    console.log("Skipping non-csv file or wrong directory:", key);
    return [];
  }

  // Skip already processed GPX files
  if (key.includes("/gpx/")) {
    console.log("Skipping GPX file:", key);
    return [];
  }

  // Check if GPX already exists
  const gpxPath = generateGPXPath(key);
  const existingGpx = await env.BUCKET.head(gpxPath);
  if (existingGpx) {
    if (!force) {
      console.log("GPX already exists, skipping:", gpxPath);
      return [];
    }
    // 同じバージョンで生成済みでも、索引に載っていなければ再生成して索引を埋める
    // （CPU時間超過でGPXだけ書けて索引更新が走らなかったケースの回復用）
    const indexed = indexedGpxKeys === undefined || indexedGpxKeys.has(gpxPath);
    if (existingGpx.customMetadata?.generatorVersion === GPX_GENERATOR_VERSION && indexed) {
      console.log("GPX already generated with current version, skipping:", gpxPath);
      return [];
    }
  }

  try {
    // Get object from R2
    const object = await env.BUCKET.get(key);
    if (!object) {
      console.error("Object not found:", key);
      return [];
    }

    // Read CSV content
    const csvText = await object.text();

    // Get timezone offset from metadata (default: +9 for JST)
    const timezoneMeta = object.customMetadata?.timezone;
    let timezoneOffset = timezoneMeta ? parseInt(timezoneMeta, 10) : 9;

    // Validate timezone offset (NaN check)
    if (isNaN(timezoneOffset)) {
      console.warn("Invalid timezone metadata, using default (JST+9):", timezoneMeta);
      timezoneOffset = 9;
    }
    console.log(
      `Timezone offset from metadata: ${timezoneOffset} (metadata: ${timezoneMeta || "not found, using default"})`
    );

    // Parse CSV
    const rawPoints = parseCSV(csvText);

    // 位置飛び（マルチパス発散・屋内ドリフト）を除去し、異常標高を補正
    const cleaned = cleanTrack(rawPoints);
    const stats: CleanStats = {
      input: rawPoints.length,
      dropped: cleaned.dropped,
      altitudeCorrected: cleaned.altitudeCorrected,
    };
    console.log(
      `Cleaned track: ${stats.input} -> ${cleaned.points.length} points ` +
        `(dropped ${stats.dropped}, altitude corrected ${stats.altitudeCorrected})`
    );
    const points = cleaned.points;

    if (points.length === 0) {
      console.log("No valid points after cleaning:", key);
      return [];
    }

    // Convert and upload GPX (may split into multiple files if too large)
    const outputs = await convertCSVToGPXAndUpload(
      points,
      gpxPath,
      env.BUCKET,
      key,
      timezoneOffset,
      stats
    );
    // 分割された場合は各パートを個別に索引化する（点数・距離・時刻はパート毎の値）
    return outputs.map((out, i) =>
      buildIndexEntry(out.points, key, out.path, timezoneOffset, stats, GPX_GENERATOR_VERSION, {
        part: i,
        parts: outputs.length,
      })
    );
  } catch (error) {
    console.error("Error processing object:", key, error);
    return [];
  }
}

/** アップロードしたGPX1ファイル分の出力情報 */
export interface GPXOutput {
  path: string;
  points: GNSSPoint[];
}

/**
 * Convert CSV points to GPX format and upload to R2
 * Handles file size limitation and splitting
 * @returns 出力したGPX（分割時は複数）とそれぞれに含めた点
 */
export async function convertCSVToGPXAndUpload(
  points: GNSSPoint[],
  basePath: string,
  bucket: R2Bucket,
  sourceFileName: string,
  timezoneOffset: number,
  stats?: CleanStats
): Promise<GPXOutput[]> {
  const outputs: GPXOutput[] = [];
  const encoder = new TextEncoder();
  // 各点のXML断片のバイト数を1回だけ計算し、上限内に収まる点数を貪欲に決める
  // （以前は二分探索で毎回GPX全体を再生成しており、4MB超のファイルでCPU時間制限に達していた）
  // 各点のXML断片のバイト数（結合時の改行1バイトを含む）
  const pointSizes = points.map(
    (p) => encoder.encode(formatTrackPoint(p, timezoneOffset)).length + 1
  );
  // ヘッダ/フッタのサイズは点数に依存しないので、空に近いGPXから見積もる（余裕を持たせる）
  const envelopeSize =
    encoder.encode(generateGPX(points.slice(0, 1), 0, sourceFileName, 99, timezoneOffset, stats))
      .length + 256;

  let fileNumber = 0;
  let currentStartIndex = 0;

  while (currentStartIndex < points.length) {
    let endIndex = currentStartIndex;
    let size = envelopeSize;
    while (endIndex < points.length && size + pointSizes[endIndex] <= MAX_GPX_FILE_SIZE) {
      size += pointSizes[endIndex];
      endIndex++;
    }
    if (endIndex === currentStartIndex) {
      // 1点も入らないことは実際には起きないが、無限ループ防止
      endIndex = currentStartIndex + 1;
    }

    const isSingleFile = fileNumber === 0 && endIndex === points.length;
    const outputPath = fileNumber === 0 ? basePath : getSplitFilePath(basePath, fileNumber);
    const segmentPoints = points.slice(currentStartIndex, endIndex);
    const gpxContent = generateGPX(
      segmentPoints,
      0,
      sourceFileName,
      fileNumber,
      timezoneOffset,
      stats
    );
    await uploadGPXToR2(bucket, outputPath, gpxContent);
    outputs.push({ path: outputPath, points: segmentPoints });

    if (isSingleFile) {
      break;
    }
    currentStartIndex = endIndex;
    fileNumber++;
  }
  return outputs;
}

/**
 * Upload GPX content to R2 (overwrites if exists)
 */
async function uploadGPXToR2(bucket: R2Bucket, path: string, gpxContent: string): Promise<void> {
  await bucket.put(path, gpxContent, {
    httpMetadata: {
      contentType: "application/gpx+xml",
    },
    customMetadata: {
      generatorVersion: GPX_GENERATOR_VERSION,
    },
  });
  console.log(`✅ Uploaded GPX: ${path}`);
}

/**
 * Generate GPX file path for split files
 */
function getSplitFilePath(basePath: string, fileNumber: number): string {
  // Replace .gpx with _N.gpx
  return basePath.replace(".gpx", `_${fileNumber}.gpx`);
}

/**
 * Generate GPX content from points
 */
export function generateGPX(
  points: GNSSPoint[],
  startIndex: number,
  sourceFileName: string,
  fileNumber: number,
  timezoneOffset: number,
  stats?: CleanStats
): string {
  if (points.length === 0) {
    throw new Error("No points to convert");
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

  // Generate track points. 記録の途切れ（SEGMENT_GAP_SEC以上）で<trkseg>を分割する
  const segments: string[] = [];
  let current: string[] = [];
  let prevEpoch: number | undefined;
  for (const p of points) {
    const epoch = toEpochSec(p.date, p.time);
    if (prevEpoch !== undefined && epoch - prevEpoch >= SEGMENT_GAP_SEC && current.length > 0) {
      segments.push(current.join("\n"));
      current = [];
    }
    current.push(formatTrackPoint(p, timezoneOffset));
    prevEpoch = epoch;
  }
  if (current.length > 0) {
    segments.push(current.join("\n"));
  }
  const trackSegments = segments.map((seg) => `    <trkseg>\n${seg}\n    </trkseg>`).join("\n");

  // クリーニング統計（後から「どれだけ捨てたか」を確認できるように残す）
  const desc = stats
    ? `\n    <desc>source=${sourceFileName} points=${stats.input} dropped=${stats.dropped} altitudeCorrected=${stats.altitudeCorrected}</desc>`
    : "";

  // Build GPX XML
  const gpx = `<?xml version="1.0" encoding="utf-8"?>
<gpx xmlns="http://www.topografix.com/GPX/1/1" version="1.0" creator="${DEFAULT_AUTHOR_NAME}">
  <metadata>
    <time>${startTime}</time>
    <bounds minlat="${minLat.toFixed(7)}" maxlat="${maxLat.toFixed(7)}" minlon="${minLng.toFixed(7)}" maxlon="${maxLng.toFixed(7)}"/>
  </metadata>
  <trk>
    <name>${DEFAULT_TITLE}${fileNumber > 0 ? ` (Part ${fileNumber + 1})` : ""}</name>${desc}
${trackSegments}
  </trk>
</gpx>`;

  return gpx;
}

/** 1点分の<trkpt>要素を生成 */
function formatTrackPoint(p: GNSSPoint, timezoneOffset: number): string {
  return `      <trkpt lat="${p.lat.toFixed(7)}" lon="${p.lng.toFixed(7)}">
        <ele>${(p.ele ?? p.alt).toFixed(1)}</ele>
        <time>${formatDateTimeForGPX(p.date, p.time, timezoneOffset)}</time>
      </trkpt>`;
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
  const isoDate = date.replace(/\//g, "-");

  // Build timezone offset string in ISO 8601 format (+HH:mm or -HH:mm)
  const tzOffsetSign = timezoneOffset >= 0 ? "+" : "-";
  const tzOffsetAbs = Math.abs(timezoneOffset);
  const tzOffsetHours = Math.floor(tzOffsetAbs);
  const tzOffsetMinutes = Math.round((tzOffsetAbs - tzOffsetHours) * 60);
  const tzOffsetStr = `${tzOffsetSign}${String(tzOffsetHours).padStart(2, "0")}:${String(tzOffsetMinutes).padStart(2, "0")}`;

  // Create ISO 8601 string with explicit timezone offset
  const localDateTimeWithTz = `${isoDate}T${time}${tzOffsetStr}`;

  // Parse with timezone - Date constructor will handle ISO 8601 with offset correctly
  const localDate = new Date(localDateTimeWithTz);

  // Format to ISO 8601 with Z suffix (UTC)
  return localDate.toISOString().replace(/\.\d{3}Z$/, "Z");
}

/**
 * Parse CSV content into array of GNSS points
 */
export function parseCSV(csvText: string): GNSSPoint[] {
  const lines = csvText.split("\n").filter((line) => line.trim() !== "");

  if (lines.length === 0) {
    return [];
  }

  // Skip header line if it contains column names
  const startIndex = lines[0].toLowerCase().includes("date") ? 1 : 0;
  const dataLines = lines.slice(startIndex);

  const points: GNSSPoint[] = [];

  for (const line of dataLines) {
    const parts = line.split(",");
    if (parts.length < 8) {
      console.log("Skipping malformed line:", line);
      continue;
    }

    // CSV format: date,time,lat,lng,alt,spd,siv,hdop[,hacc,vacc]
    // Example: 2024/01/01,12:00:00,35.6895,139.6917,50.0,5.5,8,1.2
    const lat = parseFloat(parts[2]);
    const lng = parseFloat(parts[3]);

    // Validate coordinates
    if (isNaN(lat) || isNaN(lng)) {
      console.log("Skipping invalid coordinates:", parts[2], parts[3]);
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
      hdop: parseFloat(parts[7]),
    };
    if (parts.length >= 9) {
      const hacc = parseFloat(parts[8]);
      if (!isNaN(hacc)) {
        point.hacc = hacc;
      }
    }

    points.push(point);
  }

  console.log(`Parsed ${points.length} valid points from CSV`);
  return points;
}

/**
 * Generate GPX file path from CSV file path
 */
export function generateGPXPath(csvPath: string): string {
  // Input: gnss-data/20240101/gnss_csv_data_20240102_120000.csv
  // Output: gnss-data/20240101/gpx/gnss_csv_data_20240102_120000.gpx

  const parts = csvPath.split("/");
  const fileName = parts[parts.length - 1];
  const fileNameWithoutExt = fileName.replace(".csv", "");

  // Extract date from filename (format: gnss_csv_data_YYYYMMDD_HHMMSS.csv)
  // Or from path (gnss-data/YYYYMMDD/filename.csv)
  let dateStr: string;

  // GPXはCSVと同じ日付ディレクトリの gpx/ 配下に置く。
  // 旅行中は1つの日付フォルダに複数日のCSVが入ることがあり（例: 20251225/ に 1226, 1227 のCSV）、
  // ファイル名の日付を使うとCSVと別フォルダにGPXができてビューアから辿れなくなる
  const dateMatch = fileName.match(/gnss_csv_data_(\d{8})_/);
  if (parts.length >= 2 && parts[0] === "gnss-data" && /^\d{8}$/.test(parts[1])) {
    dateStr = parts[1];
  } else if (dateMatch) {
    dateStr = dateMatch[1];
  } else {
    // Use current date
    dateStr = new Date().toISOString().slice(0, 10).replace(/-/g, "");
  }

  return `gnss-data/${dateStr}/gpx/${fileNameWithoutExt}.gpx`;
}

// Date filtering constants and functions for CONVERSION_DAYS feature

// JSTタイムゾーン定数
const JST_OFFSET_HOURS = 9;

/**
 * 現在の日時をJST（UTC+9）で取得
 */
function getCurrentDateInJST(): Date {
  const now = new Date();
  // UTCにJSTオフセットを加算（ミリ秒単位）
  const jstTime = new Date(now.getTime() + JST_OFFSET_HOURS * 60 * 60 * 1000);
  return jstTime;
}

/**
 * 直近N日分の日付文字列リストを生成（JST基準、YYYYMMDD形式）
 * @param days 日数（0以下の場合は空配列）
 * @returns 日付文字列の配列（新しい順）
 *
 * 例: days=3, 現在日時=2024-01-15 23:00:00 JST
 * 返却値: ["20240115", "20240114", "20240113"]
 */
export function generateDateRange(days: number): string[] {
  const dates: string[] = [];

  if (days <= 0) {
    return dates; // 空配列 = 全データ処理
  }

  const jstNow = getCurrentDateInJST();

  for (let i = 0; i < days; i++) {
    // i日前の日付を計算
    const date = new Date(jstNow);
    date.setDate(date.getDate() - i);

    // UTC日付としてYYYYMMDD形式にフォーマット
    const year = date.getUTCFullYear();
    const month = String(date.getUTCMonth() + 1).padStart(2, "0");
    const day = String(date.getUTCDate()).padStart(2, "0");

    dates.push(`${year}${month}${day}`);
  }

  return dates;
}

/**
 * CONVERSION_DAYS環境変数をパース
 * @param envValue 環境変数の値
 * @returns 日数（0=全データ、正数=特定日数）
 */
export function parseConversionDays(envValue: string | undefined): number {
  if (!envValue) {
    return 0; // 未設定時は全データ処理
  }

  const parsed = parseInt(envValue, 10);

  if (isNaN(parsed)) {
    console.warn(`Invalid CONVERSION_DAYS value: "${envValue}", defaulting to 0 (all data)`);
    return 0;
  }

  if (parsed < 0) {
    console.warn(`Negative CONVERSION_DAYS value: "${envValue}", defaulting to 0 (all data)`);
    return 0;
  }

  return parsed;
}
