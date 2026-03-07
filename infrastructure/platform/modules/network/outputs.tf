output "network_id" {
  value       = ovh_cloud_project_network_private.private_net.id
  description = "The ID of the private network"
}

output "subnet_id" {
  value       = ovh_cloud_project_network_private_subnet.private_subnet.id
  description = "The ID of the private subnet"
}

output "subnet_cidr" {
  value       = var.subnet_cidr
  description = "The CIDR block of the private subnet"
}

output "openstack_network_id" {
  value       = ovh_cloud_project_network_private.private_net.regions_openstack_ids[var.region]
  description = "The regional OpenStack ID of the private network"
}
