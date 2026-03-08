resource "ovh_cloud_project_kube" "kube_cluster" {
  service_name = var.service_name
  name         = var.cluster_name
  region       = var.region
  version      = var.kube_version

  private_network_id       = var.private_network_id
  nodes_subnet_id          = var.nodes_subnet_id
  load_balancers_subnet_id = var.lb_subnet_id

  private_network_configuration {
    # Set this to the first IP of your nodes_subnet (the NAT Gateway IP)
    default_vrack_gateway              = "192.168.100.1"
    private_network_routing_as_default = true
  }
}

resource "ovh_cloud_project_kube_nodepool" "kube_nodepool" {
  service_name  = var.service_name
  kube_id       = ovh_cloud_project_kube.kube_cluster.id
  name          = "${var.cluster_name}-nodepool"
  flavor_name   = var.nodepool_flavor
  desired_nodes = var.nodepool_desired_nodes
  max_nodes     = var.nodepool_max_nodes
  min_nodes     = var.nodepool_min_nodes
}
