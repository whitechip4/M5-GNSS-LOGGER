export interface Env {
  PAGES_FUNCTION_URL: string;
}

async function triggerPagesFunction(env: Env): Promise<Response> {
  try {
    // Trigger Pages Function via HTTP
    // Note: Using deployment alias URL to ensure R2 bindings work correctly
    const response = await fetch(env.PAGES_FUNCTION_URL, {
      method: "POST",
      headers: {
        "Content-Type": "application/json",
      },
    });

    if (!response.ok) {
      const errorText = await response.text();
      console.error("Failed to trigger Pages Function:", errorText);
      return new Response(
        JSON.stringify({
          status: "Error",
          error: errorText,
        }),
        {
          status: 500,
          headers: { "Content-Type": "application/json" },
        }
      );
    }

    const result = await response.json();
    console.log("Processing result:", result);
    return new Response(
      JSON.stringify({
        status: "Triggered successfully",
        result: result,
      }),
      {
        headers: { "Content-Type": "application/json" },
      }
    );
  } catch (error) {
    console.error("Error triggering Pages Function:", error);
    return new Response(
      JSON.stringify({
        status: "Error",
        error: String(error),
      }),
      {
        status: 500,
        headers: { "Content-Type": "application/json" },
      }
    );
  }
}

export default {
  /**
   * Manual trigger endpoint - POST /trigger
   */
  async fetch(request: Request, env: Env, ctx: ExecutionContext): Promise<Response> {
    const url = new URL(request.url);

    // Health check endpoint
    if (url.pathname === "/test") {
      return new Response(JSON.stringify({ status: "Scheduler Worker is running" }), {
        headers: { "Content-Type": "application/json" },
      });
    }

    // Manual trigger endpoint
    if (url.pathname === "/trigger" && request.method === "POST") {
      return triggerPagesFunction(env);
    }

    return new Response(
      "GPX Converter Scheduler Worker\nEndpoints:\n  GET /test - Health check\n  Scheduled: Triggers Pages Function hourly"
    );
  },

  /**
   * Scheduled event handler - triggers Pages Function via HTTP
   */
  async scheduled(event: ScheduledEvent, env: Env, ctx: ExecutionContext): Promise<void> {
    console.log("Scheduler triggered:", event.cron);

    // Use the same trigger function
    const response = await triggerPagesFunction(env);
    console.log("Scheduled trigger complete:", await response.text());
  },
};
