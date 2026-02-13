import type { PagesFunction } from '@cloudflare/workers-types';
import { processCSVFile, type Env } from '../shared/gnss-utils';

export const onRequest: PagesFunction<Env> = async (context) => {
  const { env, request } = context;

  console.log('Pages Function triggered:', request.method, request.url);

  // List all CSV files in gnss-data/
  const listed = await env.BUCKET.list({ prefix: 'gnss-data/' });
  console.log('Found', listed.objects.length, 'objects');

  let processed = 0;
  let skipped = 0;

  for (const object of listed.objects) {
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
    total: listed.objects.length
  }), {
    headers: { 'Content-Type': 'application/json' }
  });
};
