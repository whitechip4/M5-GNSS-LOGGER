import type { PagesFunction } from "@cloudflare/workers-types";
import {
  type Env,
  GPX_GENERATOR_VERSION,
  generateDateRange,
  parseConversionDays,
  processCSVFile,
} from "../shared/gnss-utils";
import {
  indexUnindexedGPX,
  loadTrackIndex,
  mergeTrackIndex,
  type TrackIndexEntry,
} from "../shared/track-index";

/** force時に1リクエストで変換するファイル数の既定値（CPU時間制限対策） */
const DEFAULT_FORCE_LIMIT = 5;

/** CSVかつgpx/配下でないキーか（変換対象の粗い判定。GPX有無は見ない） */
function isConversionTarget(key: string): boolean {
  return key.startsWith("gnss-data/") && key.endsWith(".csv") && !key.includes("/gpx/");
}

/**
 * List all objects in R2 bucket with continuation token support
 * R2's list() method returns max 1000 objects per call, so we need to paginate
 */
async function listAllObjects(bucket: R2Bucket, prefix: string): Promise<R2Object[]> {
  const allObjects: R2Object[] = [];
  let continuationToken: string | undefined;

  do {
    const listed = await bucket.list({
      prefix,
      cursor: continuationToken,
      limit: 1000,
    });

    allObjects.push(...listed.objects);
    console.log(`Listed ${listed.objects.length} objects (total so far: ${allObjects.length})`);

    continuationToken = listed.truncated ? listed.cursor : undefined;
  } while (continuationToken);

  return allObjects;
}

/**
 * 複数の日付ディレクトリからオブジェクトをリスト
 * @param bucket R2Bucketインスタンス
 * @param dateDates YYYYMMDD形式の日付文字列配列
 * @returns 全てのR2オブジェクト
 */
async function listObjectsByDateRange(bucket: R2Bucket, dateDates: string[]): Promise<R2Object[]> {
  const allObjects: R2Object[] = [];

  for (const dateStr of dateDates) {
    const prefix = `gnss-data/${dateStr}/`;
    console.log(`Fetching objects for date: ${dateStr}`);

    try {
      const objects = await listAllObjects(bucket, prefix);

      if (objects.length === 0) {
        console.log(`No objects found for date: ${dateStr} (directory may not exist)`);
      } else {
        allObjects.push(...objects);
        console.log(`Found ${objects.length} objects for date: ${dateStr}`);
      }
    } catch (error) {
      console.error(`Error listing objects for date ${dateStr}:`, error);
      // 次の日付の処理を継続
    }
  }

  return allObjects;
}

export const onRequest: PagesFunction<Env> = async (context) => {
  const { env, request } = context;

  console.log("Pages Function triggered:", request.method, request.url);

  // クエリパラメータ:
  //   force=1        既存GPXを上書きして再生成（クリーナー更新後の一括再変換用）
  //   days=N         環境変数CONVERSION_DAYSを一時的に上書き（0=全期間）
  //   date=YYYYMMDD  その日付ディレクトリだけ処理（daysより優先）
  //   limit=N        1リクエストで変換する最大ファイル数（CPU時間制限=エラー1102対策。
  //                  force時のデフォルト5、通常時は無制限）。応答の remaining が0になるまで繰り返し呼ぶ
  const url = new URL(request.url);
  const force = url.searchParams.get("force") === "1" || url.searchParams.get("force") === "true";
  const daysParam = url.searchParams.get("days");
  const dateParam = url.searchParams.get("date");
  const limitParam = url.searchParams.get("limit");
  const conversionDays = parseConversionDays(daysParam ?? env.CONVERSION_DAYS);
  let limit = limitParam !== null ? parseInt(limitParam, 10) : force ? DEFAULT_FORCE_LIMIT : 0;
  if (Number.isNaN(limit) || limit < 0) {
    limit = 0;
  }
  if (force) {
    console.log("Force mode: existing GPX files will be regenerated");
  }

  let allObjects: R2Object[];

  if (dateParam && /^\d{8}$/.test(dateParam)) {
    console.log(`Processing single date: ${dateParam}`);
    allObjects = await listObjectsByDateRange(env.BUCKET, [dateParam]);
  } else if (conversionDays === 0) {
    // 後方互換性: 環境変数未設定時は全データ処理
    console.log("CONVERSION_DAYS not set or set to 0, processing all data");
    allObjects = await listAllObjects(env.BUCKET, "gnss-data/");
  } else {
    // 日付範囲を生成して各日付ディレクトリを処理
    const dateRange = generateDateRange(conversionDays);
    console.log(`Processing last ${conversionDays} days:`, dateRange.join(", "));
    allObjects = await listObjectsByDateRange(env.BUCKET, dateRange);
  }

  console.log("Found total", allObjects.length, "objects to process");

  let processed = 0;
  let skipped = 0;
  let remaining = 0;
  const indexEntries: TrackIndexEntry[] = [];
  // 索引を先に読む。force時は索引に無いGPXも再生成対象にし、
  // CSVの無いGPX（手動アップロード分）は本文から索引を後追い生成する
  // 「索引済み」は現行バージョンで登録されたものだけを指す
  // （CPU時間超過でGPXだけ更新され索引が古い版のまま残ったケースも再生成対象にする）
  const index = await loadTrackIndex(env.BUCKET);
  const indexedGpxKeys = new Set(
    Object.values(index.entries)
      .filter((e) => e.source === "gpx" || e.generatorVersion === GPX_GENERATOR_VERSION)
      .map((e) => e.gpxKey)
  );

  for (const object of allObjects) {
    if (limit > 0 && processed >= limit) {
      // 変換対象（GPXなし、またはforce）だけを残件として数える
      if (isConversionTarget(object.key)) {
        remaining++;
      }
      continue;
    }
    const result = await processCSVFile(object.key, env, force, indexedGpxKeys);
    if (result.length > 0) {
      processed++;
      indexEntries.push(...result);
    } else {
      skipped++;
    }
  }

  // CSVから変換されなかったGPX（手動アップロード等）を本文から索引化するフォールバック
  // 今回変換した分は indexEntries に入っているので除外する
  const producedKeys = new Set(indexEntries.map((e) => e.gpxKey));
  const gpxKeys = allObjects
    .map((o) => o.key)
    .filter((k) => k.includes("/gpx/") && k.endsWith(".gpx") && !producedKeys.has(k));
  const fallbackBudget = limit > 0 ? Math.max(0, limit - processed) : 0;
  let indexedFromGpx = 0;
  if (fallbackBudget > 0 || limit === 0) {
    const fallback = await indexUnindexedGPX(
      env.BUCKET,
      gpxKeys,
      indexedGpxKeys,
      limit === 0 ? 0 : fallbackBudget
    );
    indexEntries.push(...fallback.entries);
    indexedFromGpx = fallback.entries.length;
    remaining += fallback.remaining;
  } else {
    remaining += gpxKeys.filter((k) => !indexedGpxKeys.has(k)).length;
  }

  // ビューア用の索引（始点の国・開始時刻・距離など）を更新
  let indexTotal = 0;
  try {
    indexTotal = await mergeTrackIndex(env.BUCKET, indexEntries);
  } catch (error) {
    console.error("Failed to update track index:", error);
  }

  return new Response(
    JSON.stringify({
      status: "Processing complete",
      processed: processed,
      skipped: skipped,
      total: allObjects.length,
      conversionDays: dateParam ?? (conversionDays === 0 ? "all" : conversionDays),
      force: force,
      limit: limit === 0 ? "none" : limit,
      remaining: remaining,
      indexEntries: indexTotal,
      indexedFromGpx: indexedFromGpx,
    }),
    {
      headers: { "Content-Type": "application/json" },
    }
  );
};
