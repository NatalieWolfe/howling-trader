CREATE TABLE howling_version (
  v                 INT NOT NULL,
  updater_id        BIGINT NULL,
  update_started_at TIMESTAMP NULL,
  updated_at        TIMESTAMP NOT NULL
);

CREATE TABLE auth_tokens (
  service_name      TEXT PRIMARY KEY,
  refresh_token     BYTEA NOT NULL,
  notice_token      VARCHAR(16) NULL,
  last_notified_at  TIMESTAMP NULL,
  updated_at        TIMESTAMP NOT NULL,
  expires_at        TIMESTAMP NULL
);

CREATE TABLE candles (
  symbol      INT NOT NULL,
  open        DOUBLE PRECISION NOT NULL,
  close       DOUBLE PRECISION NOT NULL,
  high        DOUBLE PRECISION NOT NULL,
  low         DOUBLE PRECISION NOT NULL,
  volume      BIGINT NOT NULL,
  opened_at   TIMESTAMP NOT NULL,
  duration_us BIGINT NOT NULL,
  PRIMARY KEY (symbol, opened_at)
);

CREATE TABLE market (
  symbol      INT NOT NULL,
  bid         DOUBLE PRECISION NOT NULL,
  bid_lots    BIGINT NOT NULL,
  ask         DOUBLE PRECISION NOT NULL,
  ask_lots    BIGINT NOT NULL,
  last        DOUBLE PRECISION NOT NULL,
  last_lots   BIGINT NOT NULL,
  emitted_at  TIMESTAMP NOT NULL,
  PRIMARY KEY (symbol, emitted_at)
);

CREATE TABLE trades (
  symbol      INT NOT NULL,
  executed_at TIMESTAMP NOT NULL,
  action      INT NOT NULL,
  price       DOUBLE PRECISION NOT NULL,
  quantity    BIGINT NOT NULL,
  confidence  DOUBLE PRECISION NOT NULL,
  dry_run     BOOLEAN NOT NULL
);

CREATE INDEX idx_trades_symbol_executed_at_desc
ON trades (symbol, executed_at DESC);

CREATE TABLE enum_symbols (
  id   INT PRIMARY KEY,
  name VARCHAR(24) NOT NULL
);

CREATE TABLE enum_actions (
  id   INT PRIMARY KEY,
  name VARCHAR(24) NOT NULL
);

CREATE VIEW v_trades AS
SELECT
  t.executed_at,
  t.symbol AS symbol_id,
  COALESCE(s.name, 'UNKNOWN') AS symbol,
  t.action AS action_id,
  COALESCE(a.name, 'UNKNOWN') AS action,
  t.price,
  t.quantity,
  t.confidence,
  t.dry_run
FROM trades t
LEFT JOIN enum_symbols s ON t.symbol = s.id
LEFT JOIN enum_actions a ON t.action = a.id;

-- VERSION INSERT
INSERT INTO howling_version (v, updater_id, update_started_at, updated_at)
VALUES (5, NULL, NULL, CURRENT_TIMESTAMP);
