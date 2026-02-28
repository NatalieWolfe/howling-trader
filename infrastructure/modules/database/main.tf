resource "ovh_cloud_project_database" "postgres" {
  service_name = var.service_name
  description  = var.db_name
  engine       = var.db_engine
  version      = var.db_version
  plan         = var.db_plan
  flavor       = var.db_flavor

  nodes {
    region     = var.region
    network_id = var.network_id
    subnet_id  = var.subnet_id
  }

  dynamic "ip_restrictions" {
    for_each = var.authorized_subnets
    content {
      ip          = ip_restrictions.value
      description = "Authorized subnet"
    }
  }
}

resource "ovh_cloud_project_database_database" "main" {
  service_name = var.service_name
  engine       = var.db_engine
  cluster_id   = ovh_cloud_project_database.postgres.id
  name         = "howling"
}

resource "ovh_cloud_project_database_postgresql_user" "app_user" {
  service_name = var.service_name
  cluster_id   = ovh_cloud_project_database.postgres.id
  name         = "howling_app"
}

resource "ovh_cloud_project_database_postgresql_user" "admin" {
  service_name = var.service_name
  cluster_id   = ovh_cloud_project_database.postgres.id
  name         = "avnadmin"
}
