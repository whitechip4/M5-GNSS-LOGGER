export async function onRequest(context) {
  const url = new URL(context.request.url);

  console.log('Middleware called:', url.pathname);

  // Test endpoint
  if (url.pathname === '/test') {
    return new Response('Middleware test successful');
  }

  // Continue to next handler
  return context.next();
}
