resource "ovh_cloud_project_kube" "kube_cluster" {
  service_name = var.service_name
  name         = var.cluster_name
  region       = var.region
  version      = var.kube_version
  private_network_id = var.private_network_id
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
