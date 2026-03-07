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
