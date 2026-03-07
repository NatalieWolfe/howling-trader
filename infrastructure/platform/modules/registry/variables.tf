variable "service_name" {
  type        = string
  description = "The Public Cloud Project ID"
}

variable "region" {
  type        = string
  description = "The region for the registry"
}

variable "registry_name" {
  type        = string
  description = "The name for the managed private registry"
}

variable "registry_plan" {
  type        = string
  description = "The plan name for the registry"
}

variable "registry_user_email" {
  type        = string
  description = "The email for the registry user"
}
