resource "ovh_cloud_project_network_private" "private_net" {
  service_name = var.ovh_project_id
  vlan_id      = var.vlan_id
  name         = var.network_name
  regions      = [var.region]
}

resource "ovh_cloud_project_network_private_subnet" "nodes_subnet" {
  service_name = var.ovh_project_id
  network_id   = ovh_cloud_project_network_private.private_net.id
  region       = var.region
  start        = "10.0.0.2"
  end          = "10.0.255.254"
  network      = "10.0.0.0/16"
  dhcp         = true
}

# resource "ovh_cloud_project_gateway" "gateway" {
#   service_name = var.ovh_project_id
#   name         = "howling_gateway"
#   model        = "s"
#   region       = var.region
#   network_id   = tolist(ovh_cloud_project_network_private.private_net.regions_attributes[*].openstackid)[0]
#   subnet_id    = ovh_cloud_project_network_private_subnet.nodes_subnet.id
# }
