variable "github_app_id" {
  type        = string
  description = "The GitHub App ID for ARC"
}

variable "github_app_installation_id" {
  type        = string
  description = "The GitHub App Installation ID for ARC"
}

variable "github_app_private_key" {
  type        = string
  sensitive   = true
  description = "The GitHub App Private Key (PEM format)"
}

variable "github_repo_url" {
  type        = string
  description = "The URL of the GitHub repository to register the runner to"
}

variable "system_pool_name" {
  type        = string
  description = "The name of the system node pool"
}

variable "runner_pool_name" {
  type        = string
  description = "The name of the runner node pool"
}
