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
