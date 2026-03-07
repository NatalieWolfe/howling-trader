resource "ovh_cloud_project_network_private" "private_net" {
  service_name = var.service_name
  name         = var.network_name
  regions      = [var.region]
  vlan_id      = var.vlan_id
}

resource "ovh_cloud_project_network_private_subnet" "private_subnet" {
  service_name = var.service_name
  network_id   = ovh_cloud_project_network_private.private_net.id
  region       = var.region
  start        = var.subnet_start
  end          = var.subnet_end
  network      = var.subnet_cidr
  dhcp         = true
}
