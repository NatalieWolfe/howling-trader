moved {
  from = module.database.kubernetes_secret.registry_creds
  to   = kubernetes_secret.registry_creds["howling-admin"]
}
moved {
  from = module.oauth.kubernetes_secret.registry_creds
  to   = kubernetes_secret.registry_creds["howling-app"]
}
