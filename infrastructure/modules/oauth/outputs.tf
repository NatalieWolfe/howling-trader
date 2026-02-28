output "ingress_ip" {
  value       = try(kubernetes_ingress_v1.oauth.status[0].load_balancer[0].ingress[0].ip, "pending")
  description = "The public IP address of the OAuth service ingress"
}
