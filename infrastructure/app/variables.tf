variable "ovh_endpoint" {
  type    = string
  default = "ovh-us"
}

variable "ovh_application_key" {
  type      = string
  sensitive = true
}

variable "ovh_application_secret" {
  type      = string
  sensitive = true
}

variable "ovh_consumer_key" {
  type      = string
  sensitive = true
}

variable "service_name" {
  type        = string
  description = "The Public Cloud Project ID"
}

variable "region" {
  type    = string
  default = "US-EAST-VA-1"
}

variable "registry_name" {
  type    = string
  default = "howling-registry"
}

variable "state_bucket_name" {
  type    = string
  default = "howling-trader-tofu-state"
}

variable "image_tag" {
  type        = string
  default     = "latest"
  description = "The tag for the container images"
}

variable "vault_jwt" {
  type        = string
  description = "The JWT token for Vault authentication"
  sensitive   = true
  default     = ""
}

variable "vault_address" {
  type        = string
  description = "The address of the OpenBao server"
  default     = "http://openbao.security.svc.cluster.local:8200"
}
