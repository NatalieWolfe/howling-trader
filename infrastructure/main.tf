provider "ovh" {
  endpoint           = var.ovh_endpoint
  application_key    = var.ovh_application_key
  application_secret = var.ovh_application_secret
  consumer_key       = var.ovh_consumer_key
}

provider "vault" {
  address = "http://127.0.0.1:8200"
}

locals {
  clean_region    = replace(var.region, "-1", "")
  registry_server = split("/", module.registry.registry_url)[2]

  # Decoded Vault secrets
  registry_creds       = jsondecode(vault_generic_secret.registry.data_json)
  database_creds       = jsondecode(vault_generic_secret.database.data_json)
  database_admin_creds = jsondecode(vault_generic_secret.database_admin.data_json)
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

resource "vault_generic_secret" "registry" {
  path = "secret/howling/prod/registry"
  data_json = jsonencode({
    username = module.registry.registry_user_login
    password = module.registry.registry_user_password
  })
  depends_on = [module.security]
}

# Configure the Harbor provider using the credentials from the module
provider "harbor" {
  url      = module.registry.registry_url
  username = module.registry.registry_user_login
  password = local.registry_creds["password"]
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
# OpenBao Secret Management
# ------------------------------------------------------------------------------

module "security" {
  source = "./modules/security"

  providers = {
    helm = helm
  }
}

data "vault_generic_secret" "certificates" {
  path       = "secret/howling/prod/certificates"
  depends_on = [module.security]
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
  registry_username  = local.registry_creds["username"]
  registry_password  = local.registry_creds["password"]
  image_repository   = "${local.registry_server}/${var.registry_name}/schema-upgrade"
  image_tag          = var.image_tag

  providers = {
    kubernetes = kubernetes
  }
}

resource "vault_generic_secret" "database" {
  path = "secret/howling/prod/database"
  data_json = jsonencode({
    username = module.database.db_user
    password = module.database.db_password
  })
  depends_on = [module.security]
}

resource "vault_generic_secret" "database_admin" {
  path = "secret/howling/prod/database/admin"
  data_json = jsonencode({
    username = module.database.db_admin_user
    password = module.database.db_admin_password
  })
  depends_on = [module.security]
}


# ------------------------------------------------------------------------------
# OAuth Service Deployment
# ------------------------------------------------------------------------------

module "oauth" {
  source                = "./modules/oauth"
  registry_server       = local.registry_server
  registry_username     = local.registry_creds["username"]
  registry_password     = local.registry_creds["password"]
  image_repository      = "${local.registry_server}/${var.registry_name}/howling-oauth"
  image_tag             = var.image_tag
  db_host               = module.database.db_host
  db_port               = module.database.db_port
  db_bootstrap_job_name = module.database.db_bootstrap_job_name
  letsencrypt_email     = data.vault_generic_secret.certificates.data["letsencrypt_email"]

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
