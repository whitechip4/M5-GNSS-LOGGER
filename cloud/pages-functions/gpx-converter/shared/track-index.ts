// gnss-data/index.json: ビューアがGPX本文を読まずに一覧へ付加情報を出すための索引
//
// 変換のたびに処理したファイルのエントリを既存の索引にマージして書き戻す。
// 1リクエスト内で1回だけ read-modify-write するので、hourly cron と手動トリガが
// 重なった場合に取りこぼす可能性はあるが、次の force 再生成で回復できる。

import { lookupCountry } from "./country-lookup";
import type { CleanStats, GNSSPoint } from "./gnss-utils";
import { distanceM, toEpochSec } from "./track-cleaner";

export const TRACK_INDEX_KEY = "gnss-data/index.json";

export interface TrackIndexEntry {
  /** GPXのR2キー（gnss-data/ から始まる完全キー） */
  gpxKey: string;
  /** 元CSVのR2キー */
  csvKey: string;
  /** CSVが置かれている日付ディレクトリ (YYYYMMDD) */
  date: string;
  /** 記録開始・終了（UTC, ISO 8601） */
  startTime: string;
  endTime: string;
  /** 始点座標 */
  startLat: number;
  startLng: number;
  /** 始点から推定した国（ISO 3166-1 alpha-2、不明は XX） */
  country: string;
  countryName: string;
  /** デバイスのタイムゾーンオフセット [時間]（CSVのメタデータ由来） */
  timezoneOffset: number;
  /** クリーニング後の点数と統計 */
  points: number;
  dropped: number;
  altitudeCorrected: number;
  /** クリーニング後トラックの総距離 [km] */
  distanceKm: number;
  /** 生成バージョンと生成時刻 */
  generatorVersion: string;
  generatedAt: string;
  /**
   * エントリの出自。"csv" は通常の変換、"gpx" は CSV の無い GPX 本文から後追いで作ったもの
   * （位置飛び除去は掛かっていない。timezoneOffset は不明なので 0）
   */
  source?: "csv" | "gpx";
  /** 4MB上限で分割された場合のパート番号（0始まり）と総パート数 */
  part?: number;
  parts?: number;
}

export interface TrackIndex {
  version: 1;
  updatedAt: string;
  entries: Record<string, TrackIndexEntry>;
}

function toIsoUtc(p: GNSSPoint, timezoneOffset: number): string {
  const epoch = toEpochSec(p.date, p.time) - timezoneOffset * 3600;
  return new Date(epoch * 1000).toISOString().replace(/\.\d{3}Z$/, "Z");
}

/** クリーニング済みの点列から索引エントリを作る */
export function buildIndexEntry(
  points: GNSSPoint[],
  csvKey: string,
  gpxKey: string,
  timezoneOffset: number,
  stats: CleanStats,
  generatorVersion: string,
  split?: { part: number; parts: number }
): TrackIndexEntry {
  const first = points[0];
  const last = points[points.length - 1];
  let distance = 0;
  for (let i = 1; i < points.length; i++) {
    distance += distanceM(points[i - 1].lat, points[i - 1].lng, points[i].lat, points[i].lng);
  }
  const country = lookupCountry(first.lat, first.lng);
  const dateMatch = csvKey.match(/^gnss-data\/(\d{8})\//);
  return {
    gpxKey,
    csvKey,
    date: dateMatch ? dateMatch[1] : "",
    startTime: toIsoUtc(first, timezoneOffset),
    endTime: toIsoUtc(last, timezoneOffset),
    startLat: Number(first.lat.toFixed(6)),
    startLng: Number(first.lng.toFixed(6)),
    country: country.code,
    countryName: country.name,
    timezoneOffset,
    points: points.length,
    dropped: stats.dropped,
    altitudeCorrected: stats.altitudeCorrected,
    distanceKm: Number((distance / 1000).toFixed(2)),
    generatorVersion,
    generatedAt: new Date().toISOString(),
    source: "csv",
    ...(split && split.parts > 1 ? { part: split.part, parts: split.parts } : {}),
  };
}

/** GPX 本文から索引エントリを作る（CSV が無い手動アップロード GPX 向けのフォールバック） */
export function buildIndexEntryFromGPX(gpxText: string, gpxKey: string): TrackIndexEntry | null {
  // <trkpt lat=".." lon=".."> ... <time>..</time> を順に拾う（ele/time の有無は問わない）
  const re = /<trkpt\s+lat="([-\d.]+)"\s+lon="([-\d.]+)"[^>]*>([\s\S]*?)<\/trkpt>/g;
  const timeRe = /<time>([^<]+)<\/time>/;
  let first: { lat: number; lng: number; time: string } | undefined;
  let last: { lat: number; lng: number; time: string } | undefined;
  let prevLat = 0;
  let prevLng = 0;
  let count = 0;
  let distance = 0;
  let m: RegExpExecArray | null = re.exec(gpxText);
  while (m !== null) {
    const lat = Number(m[1]);
    const lng = Number(m[2]);
    if (Number.isFinite(lat) && Number.isFinite(lng)) {
      const t = timeRe.exec(m[3]);
      const pt = { lat, lng, time: t ? t[1] : "" };
      if (first === undefined) {
        first = pt;
      } else {
        distance += distanceM(prevLat, prevLng, lat, lng);
      }
      last = pt;
      prevLat = lat;
      prevLng = lng;
      count++;
    }
    m = re.exec(gpxText);
  }
  if (first === undefined || last === undefined) {
    return null;
  }
  const country = lookupCountry(first.lat, first.lng);
  const dateMatch = gpxKey.match(/^gnss-data\/(\d{8})\//);
  return {
    gpxKey,
    csvKey: "",
    date: dateMatch ? dateMatch[1] : "",
    startTime: first.time,
    endTime: last.time,
    startLat: Number(first.lat.toFixed(6)),
    startLng: Number(first.lng.toFixed(6)),
    country: country.code,
    countryName: country.name,
    timezoneOffset: 0,
    points: count,
    dropped: 0,
    altitudeCorrected: 0,
    distanceKm: Number((distance / 1000).toFixed(2)),
    generatorVersion: "gpx-fallback",
    generatedAt: new Date().toISOString(),
    source: "gpx",
  };
}

/**
 * 索引に載っていない GPX を本文から索引化する（CPU時間制限があるので件数を絞る）
 * @returns 作成したエントリと、まだ未索引で残っている件数
 */
export async function indexUnindexedGPX(
  bucket: R2Bucket,
  gpxKeys: string[],
  indexedKeys: Set<string>,
  limit: number
): Promise<{ entries: TrackIndexEntry[]; remaining: number }> {
  const targets = gpxKeys.filter((k) => !indexedKeys.has(k));
  const entries: TrackIndexEntry[] = [];
  let processed = 0;
  for (const key of targets) {
    if (limit > 0 && processed >= limit) {
      break;
    }
    processed++;
    try {
      const obj = await bucket.get(key);
      if (!obj) {
        continue;
      }
      const entry = buildIndexEntryFromGPX(await obj.text(), key);
      if (entry) {
        entries.push(entry);
        console.log(`Indexed from GPX: ${key} (${entry.countryName}, ${entry.points} pts)`);
      } else {
        console.log(`No track points in GPX, not indexed: ${key}`);
      }
    } catch (error) {
      console.error("Failed to index GPX:", key, error);
    }
  }
  return { entries, remaining: Math.max(0, targets.length - processed) };
}

/** 既存索引を読み込む（無い・壊れている場合は空） */
export async function loadTrackIndex(bucket: R2Bucket): Promise<TrackIndex> {
  try {
    const obj = await bucket.get(TRACK_INDEX_KEY);
    if (obj) {
      const parsed = (await obj.json()) as Partial<TrackIndex>;
      if (parsed && typeof parsed === "object" && parsed.entries) {
        return { version: 1, updatedAt: parsed.updatedAt ?? "", entries: parsed.entries };
      }
    }
  } catch (error) {
    console.warn("Failed to load track index, starting fresh:", error);
  }
  return { version: 1, updatedAt: "", entries: {} };
}

/** エントリをマージして書き戻す */
export async function mergeTrackIndex(
  bucket: R2Bucket,
  newEntries: TrackIndexEntry[]
): Promise<number> {
  if (newEntries.length === 0) {
    return 0;
  }
  const index = await loadTrackIndex(bucket);
  for (const entry of newEntries) {
    index.entries[entry.gpxKey] = entry;
  }
  index.updatedAt = new Date().toISOString();
  await bucket.put(TRACK_INDEX_KEY, JSON.stringify(index), {
    httpMetadata: { contentType: "application/json", cacheControl: "no-cache" },
  });
  console.log(
    `Track index updated: ${newEntries.length} entries merged, ${Object.keys(index.entries).length} total`
  );
  return Object.keys(index.entries).length;
}
