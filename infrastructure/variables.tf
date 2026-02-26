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

variable "registry_plan" {
  type    = string
  default = "SMALL"
}

variable "registry_user_email" {
  type        = string
  description = "The email address associated with the registry user"
}
