import type { PagesFunction } from '@cloudflare/workers-types';
import { processCSVFile, type Env } from '../shared/gnss-utils';

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
      limit: 1000
    });

    allObjects.push(...listed.objects);
    console.log(`Listed ${listed.objects.length} objects (total so far: ${allObjects.length})`);

    continuationToken = listed.truncated ? listed.cursor : undefined;
  } while (continuationToken);

  return allObjects;
}

export const onRequest: PagesFunction<Env> = async (context) => {
  const { env, request } = context;

  console.log('Pages Function triggered:', request.method, request.url);

  // List all CSV files in gnss-data/ (handles >1000 objects)
  const allObjects = await listAllObjects(env.BUCKET, 'gnss-data/');
  console.log('Found total', allObjects.length, 'objects');

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

  return new Response(JSON.stringify({
    status: 'Processing complete',
    processed: processed,
    skipped: skipped,
    total: allObjects.length
  }), {
    headers: { 'Content-Type': 'application/json' }
  });
};
