data "terraform_remote_state" "platform" {
  backend = "s3"
  config = {
    bucket       = "howling-trader-tofu-state"
    key          = "platform.tfstate"
    region       = "us-east-va"
    use_lockfile = false
    endpoint     = "https://s3.us-east-va.io.cloud.ovh.us"

    use_path_style              = true
    skip_credentials_validation = true
    skip_metadata_api_check     = true
  }
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

provider "kubernetes" {
  host                   = data.terraform_remote_state.platform.outputs.kube_endpoint
  cluster_ca_certificate = base64decode(data.terraform_remote_state.platform.outputs.kube_ca_certificate)
  exec {
    api_version = "client.authentication.k8s.io/v1"
    args        = ["project", "kube", "auth", "token", var.ovh_project_id]
    command     = "ovh"
  }
}

provider "helm" {
  kubernetes {
    host                   = data.terraform_remote_state.platform.outputs.kube_endpoint
    cluster_ca_certificate = base64decode(data.terraform_remote_state.platform.outputs.kube_ca_certificate)
    exec {
      api_version = "client.authentication.k8s.io/v1"
      args        = ["project", "kube", "auth", "token", var.ovh_project_id]
      command     = "ovh"
    }
  }
}
