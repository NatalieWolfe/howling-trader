provider "ovh" {
  endpoint           = var.ovh_endpoint
  application_key    = var.ovh_application_key
  application_secret = var.ovh_application_secret
  consumer_key       = var.ovh_consumer_key
}

provider "vault" {
  address = var.vault_address

  dynamic "auth_login" {
    for_each = var.vault_jwt != "" ? [1] : []
    content {
      path = "auth/kubernetes/login"
      parameters = {
        role = "howling-ci-role"
        jwt  = var.vault_jwt
      }
    }
  }
}

data "terraform_remote_state" "platform" {
  backend = "s3"
  config = {
    bucket       = "howling-trader-tofu-state"
    key          = "platform.tfstate"
    region       = "us-east-va"
    use_lockfile = false

    endpoint = "https://s3.us-east-va.io.cloud.ovh.us"

    use_path_style              = true
    skip_credentials_validation = true
    skip_region_validation      = true
    skip_requesting_account_id  = true
    skip_metadata_api_check     = true
    skip_s3_checksum            = true
  }
}

locals {
  clean_region     = replace(var.region, "-1", "")
  registry_server  = split("/", data.terraform_remote_state.platform.outputs.registry_url)[2]
  platform_outputs = data.terraform_remote_state.platform.outputs
  kubeconfig_attrs = data.terraform_remote_state.platform.outputs.kubeconfig_attributes[0]

  # Decoded Vault secrets
  registry_creds       = vault_kv_secret_v2.registry.data
  database_creds       = vault_kv_secret_v2.database.data
  database_admin_creds = vault_kv_secret_v2.database_admin.data
}

# Configure Kubernetes and Helm providers using the cluster outputs from platform
provider "kubernetes" {
  host                   = local.kubeconfig_attrs.host
  client_certificate     = base64decode(local.kubeconfig_attrs.client_certificate)
  client_key             = base64decode(local.kubeconfig_attrs.client_key)
  cluster_ca_certificate = base64decode(local.kubeconfig_attrs.cluster_ca_certificate)
}

provider "helm" {
  kubernetes {
    host                   = local.kubeconfig_attrs.host
    client_certificate     = base64decode(local.kubeconfig_attrs.client_certificate)
    client_key             = base64decode(local.kubeconfig_attrs.client_key)
    cluster_ca_certificate = base64decode(local.kubeconfig_attrs.cluster_ca_certificate)
  }
}

# MARK: Registry credentials

resource "vault_kv_secret_v2" "registry" {
  mount = "secret"
  name  = "howling/prod/registry"
  data_json = jsonencode({
    username = local.platform_outputs.registry_user_login
    password = local.platform_outputs.registry_user_password
  })
}

# MARK: Database

module "database" {
  source             = "./modules/database"
  service_name       = var.service_name
  region             = local.clean_region
  network_id         = local.platform_outputs.openstack_network_id
  subnet_id          = local.platform_outputs.subnet_id
  authorized_subnets = [local.platform_outputs.subnet_cidr]
  registry_server    = local.registry_server
  registry_username  = local.registry_creds["username"]
  registry_password  = local.registry_creds["password"]
  image_repository   = "${local.registry_server}/${var.registry_name}/schema-upgrade"
  image_tag          = var.image_tag

  providers = {
    kubernetes = kubernetes
    ovh        = ovh
  }
}

resource "vault_kv_secret_v2" "database" {
  mount = "secret"
  name  = "howling/prod/database"
  data_json = jsonencode({
    username = module.database.db_user
    password = module.database.db_password
  })
}

resource "vault_kv_secret_v2" "database_admin" {
  mount = "secret"
  name  = "howling/prod/database/admin"
  data_json = jsonencode({
    username = module.database.db_admin_user
    password = module.database.db_admin_password
  })
}

# MARK: OAuth

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

  providers = {
    kubernetes = kubernetes
    helm       = helm
  }
}
