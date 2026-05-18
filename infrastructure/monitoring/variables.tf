variable "ovh_project_id" {
  type        = string
  description = "The Public Cloud Project ID"
}

variable "region" {
  type    = string
  default = "US-EAST-VA-1"
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
