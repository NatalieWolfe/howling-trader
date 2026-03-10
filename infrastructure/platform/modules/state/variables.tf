variable "ovh_project_id" {
  type        = string
  description = "The Public Cloud Project ID"
}

variable "region" {
  type        = string
  description = "The region for state storage"
}

variable "tofu_state_bucket_name" {
  type        = string
  description = "The name of the state storage bucket"
}
