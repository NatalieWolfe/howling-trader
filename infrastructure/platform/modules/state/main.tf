resource "ovh_cloud_project_user" "state_user" {
  service_name = var.service_name
  description  = "User for Tofu remote state management"
  role_names   = ["objectstore_operator"]
}

resource "ovh_cloud_project_user_s3_credential" "state_s3_creds" {
  service_name = var.service_name
  user_id      = ovh_cloud_project_user.state_user.id
}

resource "ovh_cloud_project_storage" "state_bucket" {
  service_name = var.service_name
  region_name  = var.region
  name         = var.state_bucket_name
  versioning = {
    status = "enabled"
  }
}
