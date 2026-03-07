variable "service_name" {
  type        = string
  description = "The Public Cloud Project ID"
}

variable "region" {
  type        = string
  description = "The region for the Kube cluster"
}

variable "cluster_name" {
  type        = string
  default     = "howling-trader-cluster"
  description = "The name for the Kube cluster"
}

variable "kube_version" {
  type    = string
  default = "1.34"
}

variable "nodepool_flavor" {
  type        = string
  default     = "b2-7"
  description = "The flavor for the cluster's worker nodes"
}

variable "nodepool_desired_nodes" {
  type        = number
  default     = 3
  description = "The desired number of nodes for the cluster"
}

variable "nodepool_max_nodes" {
  type        = number
  default     = 5
  description = "The maximum number of nodes for the cluster"
}

variable "nodepool_min_nodes" {
  type        = number
  default     = 2
  description = "The minimum number of nodes for the cluster"
}

variable "private_network_id" {
  type        = string
  description = "The ID of the private network to associate with the cluster"
}
