provider "ovh" {
  endpoint           = var.ovh_endpoint
  application_key    = var.ovh_application_key
  application_secret = var.ovh_application_secret
  consumer_key       = var.ovh_consumer_key
}

# Find the plan ID for the region and desired tier
data "ovh_cloud_project_capabilities_containerregistry_filter" "registry_capabilities" {
  service_name = var.service_name
  plan_name    = var.registry_plan
  region       = var.region
}

# Create the Managed Private Registry
resource "ovh_cloud_project_containerregistry" "registry" {
  service_name = var.service_name
  plan_id      = data.ovh_cloud_project_capabilities_containerregistry_filter.registry_capabilities.id
  region       = data.ovh_cloud_project_capabilities_containerregistry_filter.registry_capabilities.region
  name         = var.registry_name
}

# Create a default user for the registry
resource "ovh_cloud_project_containerregistry_user" "registry_user" {
  service_name = var.service_name
  registry_id  = ovh_cloud_project_containerregistry.registry.id
  login        = "howling_bot"
  email        = var.registry_user_email
}

# Configure the Harbor provider using the credentials created above
provider "harbor" {
  url      = ovh_cloud_project_containerregistry.registry.url
  username = ovh_cloud_project_containerregistry_user.registry_user.login
  password = ovh_cloud_project_containerregistry_user.registry_user.password
}

# Create a Harbor project named "howling-registry"
resource "harbor_project" "main_project" {
  name                   = var.registry_name
  public                 = false
  vulnerability_scanning = true
}
