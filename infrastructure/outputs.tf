output "registry_url" {
  value       = ovh_cloud_project_containerregistry.registry.url
  description = "The URL of the Managed Private Registry"
}

output "registry_user_login" {
  value       = ovh_cloud_project_containerregistry_user.registry_user.login
  description = "The login for the registry user"
}

output "registry_user_password" {
  value       = ovh_cloud_project_containerregistry_user.registry_user.password
  sensitive   = true
  description = "The password for the registry user"
}

output "state_s3_access_key" {
  value       = ovh_cloud_project_user_s3_credential.state_s3_creds.access_key_id
  description = "The S3 Access Key for remote state"
}

output "state_s3_secret_key" {
  value       = ovh_cloud_project_user_s3_credential.state_s3_creds.secret_access_key
  sensitive   = true
  description = "The S3 Secret Key for remote state"
}

output "state_bucket_endpoint" {
  value       = "s3.${var.region}.io.cloud.ovh.us"
  description = "The S3 endpoint for the state bucket"
}
