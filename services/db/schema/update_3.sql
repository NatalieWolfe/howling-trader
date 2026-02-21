ALTER TABLE auth_tokens ADD COLUMN last_notified_at TIMESTAMP;

UPDATE howling_version SET v = 3, updated_at = CURRENT_TIMESTAMP;
