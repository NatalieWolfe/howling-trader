output "grafana_ingress_ip" {
  value = try(
    kubernetes_ingress_v1.grafana.status[0]
    .load_balancer[0].ingress[0].ip,
    kubernetes_ingress_v1.grafana.status[0]
    .load_balancer[0].ingress[0].hostname,
    "pending"
  )
  description = "The public IP address or hostname of the Grafana ingress"
}
