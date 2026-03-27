terraform {
  required_providers {
    ovh = {
      source  = "ovh/ovh"
      version = "~> 2.12"
    }
    vault = {
      source  = "hashicorp/vault"
      version = "~> 3.0"
    }
  }
}
