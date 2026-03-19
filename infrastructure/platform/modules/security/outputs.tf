output "db_encryption_key_name" {
  value       = vault_transit_secret_backend_key.howling_db_key.name
  description = "Name of the encryption key used by the database."
}
