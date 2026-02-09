CREATE TABLE auth_tokens (
  service_name  TEXT PRIMARY KEY,
  refresh_token BYTEA NOT NULL,
  updated_at    TIMESTAMP NOT NULL
);

UPDATE howling_version SET v = 2, updated_at = CURRENT_TIMESTAMP;
