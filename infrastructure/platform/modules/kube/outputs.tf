output "cluster_id" {
  value       = ovh_cloud_project_kube.kube_cluster.id
  description = "The ID of the Kube cluster"
}

output "kubeconfig" {
  value       = ovh_cloud_project_kube.kube_cluster.kubeconfig
  sensitive   = true
  description = "The Kubeconfig for the cluster"
}

output "kubeconfig_attributes" {
  value       = ovh_cloud_project_kube.kube_cluster.kubeconfig_attributes
  sensitive   = true
  description = "The Kubeconfig attributes for the cluster"
}
