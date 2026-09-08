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

UPDATE howling_version SET v = 5, updated_at = CURRENT_TIMESTAMP;
