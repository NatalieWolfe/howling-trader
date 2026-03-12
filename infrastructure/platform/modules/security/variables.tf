variable "letsencrypt_email" {
  type        = string
  description = "The email address for Let's Encrypt"
}

variable "kube_host" {
  type        = string
  description = "Kubernetes API host"
}

variable "kube_ca_cert" {
  type        = string
  description = "Kubernetes CA certificate"
}

variable "runner_namespace" {
  type        = string
  description = "Namespace where the runner is deployed"
}
