variable "domain_name" {
  type        = string
  default     = "howling-oauth.wolfe.dev"
  description = "The domain name for the OAuth service"
}

variable "image_repository" {
  type        = string
  description = "The repository URL for the OAuth service image"
}

variable "image_tag" {
  type        = string
  default     = "latest"
  description = "The tag for the OAuth service image"
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

variable "db_uri" {
  type        = string
  sensitive   = true
  description = "The database connection URI"
}

variable "db_host" {
  type        = string
  description = "The database hostname"
}

variable "db_port" {
  type        = string
  description = "The database port"
}

variable "db_user" {
  type        = string
  description = "The database username"
}

variable "db_password" {
  type        = string
  sensitive   = true
  description = "The database password"
}

variable "letsencrypt_email" {
  type        = string
  description = "The email address for Let's Encrypt"
}
