variable "namespace" {
  type        = string
  description = "The namespace to deploy Kubernetes resources into"
}

variable "executor_image_repository" {
  type    = string
  default = "howling-executor"
}

variable "image_tag" {
  type        = string
  description = "The tag for the latest images."
}

variable "registry_server" {
  type        = string
  description = "The registry server URL"
}

variable "registry_credentials" {
  type        = string
  description = "The credentials to the container image registry."
}

variable "registry_name" {
  type        = string
  description = "The container image registry."
}

variable "auth_service_address" {
  type        = string
  description = "Address for the auth service."
}

variable "db_host" {
  type        = string
  description = "The database hostname"
}

variable "db_port" {
  type        = string
  description = "The database port"
}

variable "db_encryption_key_name" {
  type        = string
  description = "Name of the encryption key used by the database."
}

variable "openbao_agent_image" {
  type        = string
  description = "The image to use for the OpenBao agent sidecar"
}
