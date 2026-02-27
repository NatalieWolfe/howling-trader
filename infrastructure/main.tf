provider "ovh" {
  endpoint           = var.ovh_endpoint
  application_key    = var.ovh_application_key
  application_secret = var.ovh_application_secret
  consumer_key       = var.ovh_consumer_key
}

# ------------------------------------------------------------------------------
# State Management (Managed by Tofu)
# ------------------------------------------------------------------------------

module "state" {
  source            = "./modules/state"
  service_name      = var.service_name
  region            = var.region
  state_bucket_name = var.state_bucket_name
}

# ------------------------------------------------------------------------------
# Managed Private Registry
# ------------------------------------------------------------------------------

module "registry" {
  source              = "./modules/registry"
  service_name        = var.service_name
  region              = var.region
  registry_name       = var.registry_name
  registry_plan       = var.registry_plan
  registry_user_email = var.registry_user_email
}

# Configure the Harbor provider using the credentials from the module
provider "harbor" {
  url      = module.registry.registry_url
  username = module.registry.registry_user_login
  password = module.registry.registry_user_password
}

# Create a Harbor project named after the registry
module "harbor" {
  source        = "./modules/harbor"
  registry_name = var.registry_name

  providers = {
    harbor = harbor
  }
}

# ------------------------------------------------------------------------------
# Resource Refactoring (Moved Blocks)
# ------------------------------------------------------------------------------

moved {
  from = ovh_cloud_project_user.state_user
  to   = module.state.ovh_cloud_project_user.state_user
}

moved {
  from = ovh_cloud_project_user_s3_credential.state_s3_creds
  to   = module.state.ovh_cloud_project_user_s3_credential.state_s3_creds
}

moved {
  from = ovh_cloud_project_storage.state_bucket
  to   = module.state.ovh_cloud_project_storage.state_bucket
}

moved {
  from = ovh_cloud_project_containerregistry.registry
  to   = module.registry.ovh_cloud_project_containerregistry.registry
}

moved {
  from = ovh_cloud_project_containerregistry_user.registry_user
  to   = module.registry.ovh_cloud_project_containerregistry_user.registry_user
}

moved {
  from = harbor_project.main_project
  to   = module.harbor.harbor_project.main_project
}
