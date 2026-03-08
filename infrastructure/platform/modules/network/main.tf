resource "ovh_cloud_project_network_private" "private_net" {
  service_name = var.service_name
  name         = var.network_name
  regions      = [var.region]
  vlan_id      = var.vlan_id
}

# Subnet for K8s Nodes
resource "ovh_cloud_project_network_private_subnet" "nodes_subnet" {
  service_name = var.service_name
  network_id   = ovh_cloud_project_network_private.private_net.id
  region       = var.region
  start        = "192.168.100.10"
  end          = "192.168.100.200"
  network      = "192.168.100.0/24"
  dhcp         = true
  no_gateway   = false # Reverted to false to satisfy "gateway IP" requirement
}

# Subnet for K8s Load Balancers (Ingress)
resource "ovh_cloud_project_network_private_subnet" "lb_subnet" {
  service_name = var.service_name
  network_id   = ovh_cloud_project_network_private.private_net.id
  region       = var.region
  start        = "192.168.110.10"
  end          = "192.168.110.200"
  network      = "192.168.110.0/24"
  dhcp         = true
  no_gateway   = false
}

resource "ovh_cloud_project_gateway" "gateway" {
  service_name = var.service_name
  name         = "howling-nat-gateway"
  model        = "s"
  region       = var.region
  network_id   = tolist(ovh_cloud_project_network_private.private_net.regions_attributes[*].openstackid)[0]
  subnet_id    = ovh_cloud_project_network_private_subnet.nodes_subnet.id
}
