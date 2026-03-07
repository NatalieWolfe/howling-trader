output "s3_access_key" {
  value       = ovh_cloud_project_user_s3_credential.state_s3_creds.access_key_id
  description = "The S3 Access Key for remote state"
}

output "s3_secret_key" {
  value       = ovh_cloud_project_user_s3_credential.state_s3_creds.secret_access_key
  sensitive   = true
  description = "The S3 Secret Key for remote state"
}

output "bucket_endpoint" {
  value       = "s3.${var.region}.io.cloud.ovh.us"
  description = "The S3 endpoint for the state bucket"
}
