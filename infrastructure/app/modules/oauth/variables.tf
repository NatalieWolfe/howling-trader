variable "domain_name" {
  type        = string
  default     = "howling-oauth.wolfe.dev"
  description = "The domain name for the OAuth service"
}

variable "namespace" {
  type        = string
  description = "The namespace to deploy Kubernetes resources into"
}

variable "image_tag" {
  type        = string
  description = "The tag for the latest images."
}

variable "oauth_service_repository" {
  type    = string
  default = "howling-oauth"
}

variable "auth_refresh_repository" {
  type    = string
  default = "auth-refresh"
}

variable "registry_server" {
  type        = string
  description = "The registry server URL"
}

variable "registry_username" {
  type        = string
  description = "The registry username"
}

variable "registry_password" {
  type        = string
  sensitive   = true
  description = "The registry password"
}

variable "registry_name" {
  type        = string
  description = "The container image registry."
}

variable "db_host" {
  type        = string
  description = "The database hostname"
}

variable "db_port" {
  type        = string
  description = "The database port"
}

variable "db_bootstrap_job_name" {
  type        = string
  description = "The name of the DB bootstrap job"
}

variable "db_encryption_key_name" {
  type        = string
  description = "Name of the encryption key used by the database."
}

variable "openbao_agent_image" {
  type        = string
  description = "The image to use for the OpenBao agent sidecar"
}
