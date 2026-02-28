variable "service_name" {
  type        = string
  description = "The Public Cloud Project ID"
}

variable "region" {
  type        = string
  description = "The region for the database"
}

variable "db_name" {
  type        = string
  default     = "howling-oauth-db"
  description = "The name for the managed database"
}

variable "db_engine" {
  type        = string
  default     = "postgresql"
  description = "The database engine"
}

variable "db_version" {
  type        = string
  default     = "14"
  description = "The database version"
}

variable "db_plan" {
  type        = string
  default     = "essential"
  description = "The database plan"
}

variable "db_flavor" {
  type        = string
  default     = "db1-4"
  description = "The database flavor"
}

variable "network_id" {
  type        = string
  description = "The regional OpenStack ID of the private network"
}

variable "subnet_id" {
  type        = string
  description = "The ID of the private subnet"
}
