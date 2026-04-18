output "monitoring_bucket" {
  value = {
    name        = ovh_cloud_project_storage.monitoring_logs.name
    credentials = vault_kv_secret_v2.monitoring_s3.name
  }
  sensitive = true
}
