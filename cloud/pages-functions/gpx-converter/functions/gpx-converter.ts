import type { PagesFunction } from "@cloudflare/workers-types";
import {
  type Env,
  generateDateRange,
  parseConversionDays,
  processCSVFile,
} from "../shared/gnss-utils";

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

  // 環境変数から処理対象日数を取得
  const conversionDays = parseConversionDays(env.CONVERSION_DAYS);

  let allObjects: R2Object[];

  if (conversionDays === 0) {
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

  for (const object of allObjects) {
    const result = await processCSVFile(object.key, env);
    if (result) {
      processed++;
    } else {
      skipped++;
    }
  }

  return new Response(
    JSON.stringify({
      status: "Processing complete",
      processed: processed,
      skipped: skipped,
      total: allObjects.length,
      conversionDays: conversionDays === 0 ? "all" : conversionDays,
    }),
    {
      headers: { "Content-Type": "application/json" },
    }
  );
};
