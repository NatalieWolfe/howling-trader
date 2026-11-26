# Howling Reader MCP Server

`//services/reader_mcp` provides a read-only Model Context Protocol (MCP
`2025-03-26`, Streamable HTTP transport) server over the Howling PostgreSQL
trading database. It exposes structured queries for candles, market quotes,
trades, trading summaries, and schema metadata without allowing writes or
arbitrary SQL execution.

## Local Docker Compose Endpoint

When started via `bazel run //infrastructure:services-up`, the `reader_mcp`
container binds exclusively to loopback on the host:

- **MCP JSON-RPC endpoint:** `http://127.0.0.1:8081/mcp`
- **Health check endpoint:** `http://127.0.0.1:8081/status`

## Tools

All tools are annotated with `readOnlyHint: true` and reject unknown argument
keys. `symbol` and `action` values are case-sensitive uppercase enum names
(for example, `NVDA`, `AAPL`, `BUY`, `SELL`, `HOLD`). Timestamps accept
ISO-8601 inputs with a required timezone offset (`Z`, `+HH:MM`, `-HH:MM`,
optional fractional seconds) normalized to UTC, and all responses emit UTC
timestamps ending in `Z`. Paginated tools default to `limit=100` (max `1000`),
`offset=0`, and return `"has_more": true/false`.

| Tool | Description | Arguments |
| --- | --- | --- |
| `get_candles` | Raw or time-bucketed OHLCV candles for a symbol. | `symbol` (required), `start_time`, `end_time`, `limit`, `offset`, `order` (`asc`/`desc`), `bucket_seconds` (`1..31622400`) |
| `get_market_data` | Level-1 market quotes (bid/ask/last) for a symbol. | `symbol` (required), `start_time`, `end_time`, `limit`, `offset`, `order` |
| `get_trades` | Executed trade records. | `symbol`, `action` (`BUY`/`SELL`/`HOLD`), `dry_run` (bool), `start_time`, `end_time`, `limit`, `offset`, `order` |
| `get_trading_summary` | Per-symbol trade and candle aggregates (including zero-trade symbols with candles, plus separate `buy_notional` and `sell_notional`). Defaults to the last 24 hours UTC when both `start_time` and `end_time` are omitted. | `symbol`, `dry_run` (bool), `start_time`, `end_time` |

## Resources

All resources return `application/json`:

| URI | Description |
| --- | --- |
| `howling://symbols` | Available symbols, per-symbol `min`/`max` timestamps across candles/market/trades, exact trade counts, and approximate table row counts. |
| `howling://actions` | Trade action enum definitions from `enum_actions`. |
| `howling://schema` | Database schema version and allowlisted table/view column overview. |

## Standalone CLI Flags

When running `bazel run //services/reader_mcp:server` directly outside Docker
Compose, the binary accepts:

- `--bind_address` (default `127.0.0.1`): numeric IP literal to bind.
- `--port` (default `8080`): TCP port to listen on (`0` selects an ephemeral port).
- `--pg_host`, `--pg_port`, `--pg_user`, `--pg_password`, `--pg_database`,
  `--pg_enable_encryption`: PostgreSQL connection settings.
- `--db_pool_min_size`, `--db_pool_max_size`: read-only connection pool bounds
  (HTTP worker threads are sized 1:1 with `--db_pool_max_size`).

## Client Configuration (`~/.gemini/config/mcp_config.json`)

Add the following entry under `mcpServers` in `~/.gemini/config/mcp_config.json`
to connect Antigravity (`agy`) to the local `reader_mcp` service:

```json
{
  "mcpServers": {
    "howling-reader-mcp": {
      "serverUrl": "http://127.0.0.1:8081/mcp"
    }
  }
}
```
