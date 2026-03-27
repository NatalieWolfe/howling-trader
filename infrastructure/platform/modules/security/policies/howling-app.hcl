# 1. Read-only access to application secrets (Runtime only)
path "secret/data/howling/prod/*" {
  capabilities = ["read", "list"]
}

# 2. Access to Transit engine for database encryption/decryption
path "transit/encrypt/howling-db-key" {
  capabilities = ["update"]
}

path "transit/decrypt/howling-db-key" {
  capabilities = ["update"]
}

path "pki/issue/howling-node-role" {
  capabilities = ["update"]
}
