output "oauth_ingress_ip" {
  value       = module.oauth.ingress_ip
  description = "The public IP address for the OAuth service"
}
