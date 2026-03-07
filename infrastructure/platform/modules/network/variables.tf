variable "service_name" {
  type        = string
  description = "The Public Cloud Project ID"
}

variable "region" {
  type        = string
  description = "The region for the network"
}

variable "network_name" {
  type        = string
  default     = "howling-private-net"
  description = "The name for the private network"
}

variable "vlan_id" {
  type        = number
  default     = 100
  description = "The VLAN ID for the private network"
}

variable "subnet_cidr" {
  type        = string
  default     = "192.168.10.0/24"
  description = "The CIDR block for the private subnet"
}

variable "subnet_start" {
  type        = string
  default     = "192.168.10.10"
  description = "The starting IP address for the subnet allocation pool"
}

variable "subnet_end" {
  type        = string
  default     = "192.168.10.200"
  description = "The ending IP address for the subnet allocation pool"
}
