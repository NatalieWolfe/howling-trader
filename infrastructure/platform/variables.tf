variable "ovh_project_id" {
  type        = string
  description = "The Public Cloud Project ID"
}

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

variable "region" {
  type    = string
  default = "US-EAST-VA-1"
}

variable "registry_name" {
  type    = string
  default = "howling-registry"
}

variable "registry_plan" {
  type    = string
  default = "SMALL"
}

variable "tofu_state_bucket_name" {
  type    = string
  default = "howling-trader-tofu-state"
}

variable "letsencrypt_email" {
  type        = string
  description = "The email address for Let's Encrypt"
}

variable "github_app_id" {
  type        = string
  default     = "3029744"
  description = "The GitHub App ID for ARC"
}

variable "github_app_installation_id" {
  type        = string
  default     = "114599649"
  description = "The GitHub App Installation ID for ARC"
}

variable "github_app_private_key" {
  type        = string
  sensitive   = true
  description = "The GitHub App Private Key (PEM format)"
}

variable "github_repo_url" {
  type        = string
  default     = "https://github.com/NatalieWolfe/howling-trader"
  description = "The URL of the GitHub repository to register the runner to"
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
