resource "ovh_cloud_project_user" "tofu_state_user" {
  service_name = var.ovh_project_id
  description  = "User for Tofu remote state management"
  role_names   = ["objectstore_operator"]
}

resource "ovh_cloud_project_user_s3_credential" "tofu_state_s3_creds" {
  service_name = var.ovh_project_id
  user_id      = ovh_cloud_project_user.tofu_state_user.id
}

resource "ovh_cloud_project_storage" "tofu_state_bucket" {
  service_name = var.ovh_project_id
  region_name  = var.region
  name         = var.tofu_state_bucket_name
  versioning = {
    status = "enabled"
  }
}
