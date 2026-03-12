provider "ovh" {
  endpoint           = var.ovh_endpoint
  application_key    = var.ovh_application_key
  application_secret = var.ovh_application_secret
  consumer_key       = var.ovh_consumer_key
}

locals {
  clean_region     = replace(var.region, "-1", "")
  kubeconfig_attrs = module.kube.kubeconfig_attributes[0]
}

# MARK: Tofu State

module "state" {
  source                 = "./modules/state"
  ovh_project_id         = var.ovh_project_id
  region                 = local.clean_region
  tofu_state_bucket_name = var.tofu_state_bucket_name
}

# MARK: Private Registry

module "registry" {
  source         = "./modules/registry"
  ovh_project_id = var.ovh_project_id
  region         = local.clean_region
  registry_name  = var.registry_name
  registry_plan  = var.registry_plan
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

# MARK: Network Infrastructure

module "network" {
  source         = "./modules/network"
  ovh_project_id = var.ovh_project_id
  region         = var.region
}

# MARK: Kubernetes

module "kube" {
  source     = "./modules/kube"
  depends_on = [module.network]

  ovh_project_id     = var.ovh_project_id
  region             = var.region
  private_network_id = module.network.openstack_network_id
  nodes_subnet_id    = module.network.subnet_id
  lb_subnet_id       = "" # module.network.lb_subnet_id
}

# Configure Kubernetes and Helm providers using the cluster outputs
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

provider "vault" {
  address = "http://openbao.security.svc.cluster.local:8200"

  auth_login {
    path = "auth/kubernetes/login"
    parameters = {
      role = "howling-ci-role"
      jwt  = var.vault_jwt
    }
  }
}

# MARK: OpenBao

module "security" {
  source = "./modules/security"

  letsencrypt_email = var.letsencrypt_email
  kube_host         = local.kubeconfig_attrs.host
  kube_ca_cert      = base64decode(local.kubeconfig_attrs.cluster_ca_certificate)
  runner_namespace  = module.runner.runner_namespace

  providers = {
    helm  = helm
    vault = vault
  }
}

# MARK: GitHub Runner (ARC)

module "runner" {
  source                     = "./modules/runner"
  github_app_id              = var.github_app_id
  github_app_installation_id = var.github_app_installation_id
  github_app_private_key     = var.github_app_private_key
  github_repo_url            = var.github_repo_url

  providers = {
    kubernetes = kubernetes
    helm       = helm
  }
}
