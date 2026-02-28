output "registry_url" {
  value       = module.registry.registry_url
  description = "The URL of the Managed Private Registry"
}

output "registry_user_login" {
  value       = module.registry.registry_user_login
  description = "The login for the registry user"
}

output "registry_user_password" {
  value       = module.registry.registry_user_password
  sensitive   = true
  description = "The password for the registry user"
}

output "state_s3_access_key" {
  value       = module.state.s3_access_key
  description = "The S3 Access Key for remote state"
}

output "state_s3_secret_key" {
  value       = module.state.s3_secret_key
  sensitive   = true
  description = "The S3 Secret Key for remote state"
}

output "state_bucket_endpoint" {
  value       = module.state.bucket_endpoint
  description = "The S3 endpoint for the state bucket"
}

output "oauth_ingress_ip" {
  value       = module.oauth.ingress_ip
  description = "The public IP address for the OAuth service"
}

output "kube_cluster_id" {
  value       = module.kube.cluster_id
  description = "The ID of the Managed Kubernetes cluster"
}
