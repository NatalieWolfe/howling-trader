output "db_id" {
  value       = ovh_cloud_project_database.postgres.id
  description = "The ID of the managed database"
}

output "db_host" {
  value       = ovh_cloud_project_database.postgres.endpoints[0].host
  description = "The database hostname"
}

output "db_port" {
  value       = ovh_cloud_project_database.postgres.endpoints[0].port
  description = "The database port"
}

output "db_uri" {
  value       = ovh_cloud_project_database.postgres.endpoints[0].uri
  sensitive   = true
  description = "The connection URI for the database"
}

output "db_user" {
  value       = ovh_cloud_project_database_postgresql_user.app_user.name
  description = "The database username"
}

output "db_password" {
  value       = ovh_cloud_project_database_postgresql_user.app_user.password
  sensitive   = true
  description = "The database password"
}
