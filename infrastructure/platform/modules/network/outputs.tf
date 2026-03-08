output "network_id" {
  value       = ovh_cloud_project_network_private.private_net.id
  description = "The ID of the private network"
}

output "subnet_id" {
  value       = ovh_cloud_project_network_private_subnet.nodes_subnet.id
  description = "The ID of the private subnet used for nodes"
}

output "lb_subnet_id" {
  value       = ovh_cloud_project_network_private_subnet.lb_subnet.id
  description = "The ID of the private subnet used for load balancers"
}

output "subnet_cidr" {
  value       = "192.168.100.0/24"
  description = "The CIDR block of the node subnet"
}

output "openstack_network_id" {
  value       = ovh_cloud_project_network_private.private_net.regions_openstack_ids[var.region]
  description = "The regional OpenStack ID of the private network"
}
