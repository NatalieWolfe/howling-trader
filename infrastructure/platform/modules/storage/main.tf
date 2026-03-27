# MARK: Monitoring

resource "ovh_cloud_project_storage" "monitoring_logs" {
  name         = "howling-monitoring-logs"
  service_name = var.ovh_project_id
  region_name  = var.region
}

resource "ovh_cloud_project_storage_object_bucket_lifecycle_configuration" "monitoring_logs_retention" {
  service_name   = var.ovh_project_id
  region_name    = var.region
  container_name = ovh_cloud_project_storage.monitoring_logs.name
  rules = [{
    id         = "retention-14-days"
    status     = "enabled"
    expiration = { days = 14 }
  }]
}

resource "ovh_cloud_project_user" "monitoring_s3" {
  service_name = var.ovh_project_id
  description  = "User for monitoring S3 access"
  role_name    = "objectstore_operator"
}

resource "ovh_cloud_project_user_s3_credential" "monitoring_s3" {
  service_name = var.ovh_project_id
  user_id      = ovh_cloud_project_user.monitoring_s3.id
}

resource "vault_kv_secret_v2" "monitoring_s3" {
  mount = "secret"
  name  = "howling/monitoring/s3"
  data_json = jsonencode({
    access_key = ovh_cloud_project_user_s3_credential.monitoring_s3.access_key_id
    secret_key = ovh_cloud_project_user_s3_credential.monitoring_s3.secret_access_key
  })
}
