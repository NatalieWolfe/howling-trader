path "secret/data/howling/*" {
  capabilities = ["read"]
}
path "transit/encrypt/howling-db-key" {
  capabilities = ["update"]
}
path "transit/decrypt/howling-db-key" {
  capabilities = ["update"]
}
