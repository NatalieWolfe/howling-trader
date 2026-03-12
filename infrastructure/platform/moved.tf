# Resource Migrations to prevent recreation

moved {
  from = module.kube.ovh_cloud_project_kube.kube_cluster
  to   = ovh_cloud_project_kube.kube_cluster
}

moved {
  from = module.kube.ovh_cloud_project_kube_nodepool.kube_nodepool
  to   = ovh_cloud_project_kube_nodepool.system_pool
}
