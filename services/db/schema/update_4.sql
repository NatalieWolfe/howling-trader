ALTER TABLE auth_tokens ADD COLUMN notice_token VARCHAR(16);
ALTER TABLE auth_tokens ADD COLUMN expires_at TIMESTAMP;

UPDATE howling_version SET v = 4, updated_at = CURRENT_TIMESTAMP;
