output "registry_url" {
  value       = ovh_cloud_project_containerregistry.registry.url
  description = "The URL of the registry"
}

output "registry_user_login" {
  value       = ovh_cloud_project_containerregistry_user.registry_user.login
  description = "The login of the registry user"
}

output "registry_user_password" {
  value       = ovh_cloud_project_containerregistry_user.registry_user.password
  sensitive   = true
  description = "The password of the registry user"
}
