resource "ovh_cloud_project_kube" "kube_cluster" {
  service_name       = var.ovh_project_id
  name               = var.cluster_name
  region             = var.region
  version            = var.kube_version
  private_network_id = var.private_network_id

  private_network_configuration {
    private_network_routing_as_default = true
    default_vrack_gateway              = ""
  }
}

resource "ovh_cloud_project_kube_nodepool" "kube_nodepool" {
  service_name  = var.ovh_project_id
  kube_id       = ovh_cloud_project_kube.kube_cluster.id
  name          = "${var.cluster_name}-nodepool"
  flavor_name   = var.nodepool_flavor
  desired_nodes = var.nodepool_desired_nodes
  max_nodes     = var.nodepool_max_nodes
  min_nodes     = var.nodepool_min_nodes
}
