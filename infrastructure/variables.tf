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

variable "letsencrypt_email" {
  type        = string
  description = "The email address for Let's Encrypt certificates"
}

variable "state_bucket_name" {
  type    = string
  default = "howling-trader-tofu-state"
}

variable "telegram_bot_token" {
  type        = string
  sensitive   = true
  description = "The Telegram bot token"
}

variable "telegram_chat_id" {
  type        = string
  description = "The Telegram chat ID"
}

variable "schwab_api_key" {
  type      = string
  sensitive = true
}

variable "schwab_api_secret" {
  type      = string
  sensitive = true
}

variable "image_tag" {
  type        = string
  default     = "latest"
  description = "The tag to use for all service images"
}
