CREATE TABLE howling_version (
  v                 INT NOT NULL,
  updater_id        BIGINT NULL,
  update_started_at TIMESTAMP NULL,
  updated_at        TIMESTAMP NOT NULL
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
  PRIMARY KEY (symbol, opened_at ASC) ON CONFLICT REPLACE
);

-- VERSION INSERT
INSERT INTO howling_version (v, updater_id, update_started_at, updated_at)
VALUES (1, NULL, NULL, CURRENT_TIMESTAMP);
