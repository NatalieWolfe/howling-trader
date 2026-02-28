terraform {
  required_version = ">= 1.6.0" # OpenTofu compatible

  backend "s3" {
    bucket       = "howling-trader-tofu-state"
    key          = "terraform.tfstate"
    region       = "us-east-va"
    use_lockfile = false # OVH S3 does not support conditional PUTs required for native locking

    endpoint = "https://s3.us-east-va.io.cloud.ovh.us"

    use_path_style              = true
    skip_credentials_validation = true
    skip_region_validation      = true
    skip_requesting_account_id  = true
    skip_metadata_api_check     = true
    skip_s3_checksum            = true
  }

  required_providers {
    ovh = {
      source  = "ovh/ovh"
      version = "~> 2.11"
    }
    harbor = {
      source  = "goharbor/harbor"
      version = "~> 3.10"
    }
    kubernetes = {
      source  = "hashicorp/kubernetes"
      version = "~> 2.0"
    }
    helm = {
      source  = "hashicorp/helm"
      version = "~> 2.0"
    }
  }
}
