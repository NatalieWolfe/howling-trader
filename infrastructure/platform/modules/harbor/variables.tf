variable "registry_name" {
  type        = string
  description = "The name of the Harbor project to create"
}

variable "github_username" {
  type        = string
  description = "The GitHub username for GHCR authentication"
}

variable "ghcr_readonly_token" {
  type        = string
  sensitive   = true
  description = "The GitHub PAT for GHCR authentication"
}
