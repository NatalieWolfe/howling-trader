terraform {
  required_version = ">= 1.6.0" # OpenTofu compatible
  required_providers {
    ovh = {
      source  = "ovh/ovh"
      version = "~> 2.11"
    }
  }
}
