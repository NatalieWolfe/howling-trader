data "ovh_cloud_project_capabilities_containerregistry_filter" "registry_capabilities" {
  service_name = var.service_name
  plan_name    = var.registry_plan
  region       = var.region
}

resource "ovh_cloud_project_containerregistry" "registry" {
  service_name = var.service_name
  plan_id      = data.ovh_cloud_project_capabilities_containerregistry_filter.registry_capabilities.id
  region       = data.ovh_cloud_project_capabilities_containerregistry_filter.registry_capabilities.region
  name         = var.registry_name
}

resource "ovh_cloud_project_containerregistry_user" "registry_user" {
  service_name = var.service_name
  registry_id  = ovh_cloud_project_containerregistry.registry.id
  login        = "howling_bot"
  email        = var.registry_user_email
}
