provider "ovh" {
  endpoint           = var.ovh_endpoint
  application_key    = var.ovh_application_key
  application_secret = var.ovh_application_secret
  consumer_key       = var.ovh_consumer_key
}

locals {
  clean_region    = replace(var.region, "-1", "")
  registry_server = split("/", module.registry.registry_url)[2]
}

# ------------------------------------------------------------------------------
# State Management (Managed by Tofu)
# ------------------------------------------------------------------------------

module "state" {
  source            = "./modules/state"
  service_name      = var.service_name
  region            = local.clean_region
  state_bucket_name = var.state_bucket_name
}

# ------------------------------------------------------------------------------
# Managed Private Registry
# ------------------------------------------------------------------------------

module "registry" {
  source              = "./modules/registry"
  service_name        = var.service_name
  region              = local.clean_region
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
# Network Infrastructure (vRack)
# ------------------------------------------------------------------------------

module "network" {
  source       = "./modules/network"
  service_name = var.service_name
  region       = var.region
}

# ------------------------------------------------------------------------------
# Managed Kubernetes Cluster
# ------------------------------------------------------------------------------

module "kube" {
  source             = "./modules/kube"
  service_name       = var.service_name
  region             = var.region
  private_network_id = module.network.openstack_network_id
}

# Configure Kubernetes and Helm providers using the cluster outputs
provider "kubernetes" {
  host                   = module.kube.kubeconfig_attributes[0].host
  client_certificate     = base64decode(module.kube.kubeconfig_attributes[0].client_certificate)
  client_key             = base64decode(module.kube.kubeconfig_attributes[0].client_key)
  cluster_ca_certificate = base64decode(module.kube.kubeconfig_attributes[0].cluster_ca_certificate)
}

provider "helm" {
  kubernetes {
    host                   = module.kube.kubeconfig_attributes[0].host
    client_certificate     = base64decode(module.kube.kubeconfig_attributes[0].client_certificate)
    client_key             = base64decode(module.kube.kubeconfig_attributes[0].client_key)
    cluster_ca_certificate = base64decode(module.kube.kubeconfig_attributes[0].cluster_ca_certificate)
  }
}

# ------------------------------------------------------------------------------
# Managed Database (Postgres)
# ------------------------------------------------------------------------------

module "database" {
  source             = "./modules/database"
  service_name       = var.service_name
  region             = local.clean_region
  network_id         = module.network.openstack_network_id
  subnet_id          = module.network.subnet_id
  authorized_subnets = [module.network.subnet_cidr]
  registry_server    = local.registry_server
  registry_username  = module.registry.registry_user_login
  registry_password  = module.registry.registry_user_password
  image_repository   = "${local.registry_server}/${var.registry_name}/schema-upgrade"

  providers = {
    kubernetes = kubernetes
  }
}


# ------------------------------------------------------------------------------
# OAuth Service Deployment
# ------------------------------------------------------------------------------

module "oauth" {
  source                = "./modules/oauth"
  registry_server       = local.registry_server
  registry_username     = module.registry.registry_user_login
  registry_password     = module.registry.registry_user_password
  image_repository      = "${local.registry_server}/${var.registry_name}/howling-oauth"
  db_uri                = module.database.db_uri
  db_host               = module.database.db_host
  db_port               = module.database.db_port
  db_user               = module.database.db_user
  db_password           = module.database.db_password
  db_bootstrap_job_name = module.database.db_bootstrap_job_name
  letsencrypt_email     = var.letsencrypt_email

  providers = {
    kubernetes = kubernetes
    helm       = helm
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
