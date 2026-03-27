provider "ovh" {
  endpoint           = var.ovh_endpoint
  application_key    = var.ovh_application_key
  application_secret = var.ovh_application_secret
  consumer_key       = var.ovh_consumer_key
}

locals {
  clean_region      = replace(var.region, "-1", "")
  system_pool_name  = "howling-trader-cluster-nodepool"
  runner_pool_name  = "arc-runner-nodepool"
  monitor_pool_name = "monitor-nodepool"
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
  source              = "./modules/harbor"
  registry_name       = var.registry_name
  github_username     = var.github_username
  ghcr_readonly_token = var.ghcr_readonly_token

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

resource "ovh_cloud_project_kube" "kube_cluster" {
  service_name       = var.ovh_project_id
  name               = "howling-trader-cluster"
  region             = var.region
  version            = "1.34"
  private_network_id = module.network.openstack_network_id

  private_network_configuration {
    private_network_routing_as_default = true
    default_vrack_gateway              = ""
  }

  depends_on = [module.network]
}

resource "ovh_cloud_project_kube_nodepool" "system_pool" {
  service_name = var.ovh_project_id
  kube_id      = ovh_cloud_project_kube.kube_cluster.id
  name         = local.system_pool_name
  flavor_name  = "b2-7"
  max_nodes    = 5
  min_nodes    = 3
  autoscale    = true

  template {
    metadata {
      labels = {
        nodepool = local.system_pool_name
      }
      annotations = {}
      finalizers  = []
    }
    spec {
      unschedulable = false
      taints        = []
    }
  }
}

resource "ovh_cloud_project_kube_nodepool" "runner_pool" {
  service_name = var.ovh_project_id
  kube_id      = ovh_cloud_project_kube.kube_cluster.id
  name         = local.runner_pool_name
  flavor_name  = "b2-30"
  max_nodes    = 3
  min_nodes    = 0
  autoscale    = true

  template {
    metadata {
      labels = {
        nodepool = local.runner_pool_name
      }
      annotations = {}
      finalizers  = []
    }
    spec {
      unschedulable = false
      taints        = []
    }
  }
}

resource "ovh_cloud_project_kube_nodepool" "monitor_pool" {
  service_name = var.ovh_project_id
  kube_id      = ovh_cloud_project_kube.kube_cluster.id
  name         = local.monitor_pool_name
  flavor_name  = "r2-15"
  max_nodes    = 3
  min_nodes    = 1
  autoscale    = true

  template {
    metadata {
      labels = {
        nodepool = local.monitor_pool_name
      }
      annotations = {}
      finalizers  = []
    }
    spec {
      unschedulable = false
      taints = [
        { key = "dedicated", value = "monitoring", effect = "NoSchedule" }
      ]
    }
  }
}

locals {
  kubeconfig_attrs = ovh_cloud_project_kube.kube_cluster.kubeconfig_attributes[0]
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

# MARK: OpenBao

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

module "security" {
  source = "./modules/security"

  letsencrypt_email = var.letsencrypt_email
  kube_host         = local.kubeconfig_attrs.host
  kube_ca_cert      = base64decode(local.kubeconfig_attrs.cluster_ca_certificate)
  runner_namespace  = module.runner.runner_namespace
  pool_name         = local.system_pool_name

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
  system_pool_name           = local.system_pool_name
  runner_pool_name           = local.runner_pool_name

  providers = {
    kubernetes = kubernetes
    helm       = helm
  }
}

# MARK: Storage

module "storage" {
  source         = "./modules/storage"
  ovh_project_id = var.ovh_project_id
  region         = local.clean_region

  providers = {
    ovh   = ovh
    vault = vault
  }
}
