locals {
  bao_host = "openbao.security.svc.cluster.local:8200"
}

resource "helm_release" "openbao" {
  name             = "openbao"
  repository       = "https://openbao.github.io/openbao-helm"
  chart            = "openbao"
  namespace        = "security"
  create_namespace = true

  set {
    name  = "server.ha.enabled"
    value = "true"
  }

  set {
    name  = "server.ha.replicas"
    value = "3"
  }

  set {
    name  = "server.ha.raft.enabled"
    value = "true"
  }

  # Enable the Agent Injector for sidecar support
  set {
    name  = "injector.enabled"
    value = "true"
  }

  set {
    name  = "server.nodeSelector.nodepool"
    value = var.pool_name
  }
}

# MARK: Ingress Controller

resource "helm_release" "ingress_nginx" {
  name             = "ingress-nginx"
  repository       = "https://kubernetes.github.io/ingress-nginx"
  chart            = "ingress-nginx"
  namespace        = "ingress-nginx"
  create_namespace = true

  set {
    name  = "controller.service.externalTrafficPolicy"
    value = "Local"
  }

  set {
    name  = "controller.nodeSelector.nodepool"
    value = var.pool_name
  }
}

# MARK: Cert Manager

resource "helm_release" "cert_manager" {
  name             = "cert-manager"
  repository       = "https://charts.jetstack.io"
  chart            = "cert-manager"
  namespace        = "cert-manager"
  create_namespace = true

  set {
    name  = "installCRDs"
    value = "true"
  }

  set {
    name  = "nodeSelector.nodepool"
    value = var.pool_name
  }
}

# MARK: Lets Encrypt

resource "helm_release" "letsencrypt_issuer" {
  name      = "letsencrypt-issuer"
  chart     = "${path.module}/issuer-chart"
  namespace = "cert-manager"

  set {
    name  = "email"
    value = var.letsencrypt_email
  }

  depends_on = [helm_release.cert_manager]
}

# MARK: OpenBao

resource "vault_mount" "secret" {
  path        = "secret"
  type        = "kv"
  options     = { version = "2" }
  description = "KV Version 2 secrets engine for application data"

  depends_on = [helm_release.openbao]
}

resource "vault_mount" "transit" {
  path        = "transit"
  type        = "transit"
  description = "Transit engine for DB fields"

  depends_on = [helm_release.openbao]
}

resource "vault_transit_secret_backend_key" "howling_db_key" {
  backend               = vault_mount.transit.path
  name                  = "howling-db-key"
  type                  = "aes256-gcm96"
  deletion_allowed      = true
  convergent_encryption = false
  auto_rotate_period    = 3600 * 24 * 365 # 1 year in seconds.
}

resource "vault_mount" "pki" {
  path = "pki"
  type = "pki"

  default_lease_ttl_seconds = 3600 * 24 * 365      # 1 year in seconds.
  max_lease_ttl_seconds     = 3600 * 24 * 365 * 10 # 10 years in seconds.

  depends_on = [helm_release.openbao]
}

resource "vault_pki_secret_backend_root_cert" "howling_trader_root_cert" {
  backend     = vault_mount.pki.path
  type        = "internal"
  common_name = "Howling Trader Root Cert"
  key_type    = "ec"
  key_bits    = 384
  ttl         = "${24 * 365 * 10}h" # 10 years in hours.
}

resource "vault_pki_secret_backend_config_urls" "cert_urls" {
  backend                 = vault_mount.pki.path
  issuing_certificates    = ["http://${local.bao_host}/v1/pki/ca"]
  crl_distribution_points = ["http://${local.bao_host}/v1/pki/crl"]
}

resource "vault_pki_secret_backend_role" "howling_node_role" {
  backend          = vault_mount.pki.path
  name             = "howling-node-role"
  allow_ip_sans    = true
  allow_localhost  = true
  allow_subdomains = true
  allowed_domains  = ["howling-app.svc.cluster.local"]
  max_ttl          = "${24 * 30}h" # 30 days in hours.
  server_flag      = true
  client_flag      = true
  no_store         = true
}

resource "vault_auth_backend" "kubernetes" {
  type = "kubernetes"
  path = "kubernetes"

  depends_on = [helm_release.openbao]
}

resource "vault_kubernetes_auth_backend_config" "config" {
  backend            = vault_auth_backend.kubernetes.path
  kubernetes_host    = var.kube_host
  kubernetes_ca_cert = var.kube_ca_cert

  depends_on = [vault_auth_backend.kubernetes]
}

resource "vault_policy" "ci_app" {
  name   = "howling-ci-app"
  policy = file("${path.module}/policies/howling-ci-app.hcl")
}

resource "vault_kubernetes_auth_backend_role" "ci_runner" {
  backend        = vault_auth_backend.kubernetes.path
  role_name      = "howling-ci-role"
  token_policies = [vault_policy.ci_app.name]
  token_ttl      = 3600

  bound_service_account_names = ["*"]
  bound_service_account_namespaces = [
    var.runner_namespace,
    "howling-admin"
  ]

  depends_on = [vault_kubernetes_auth_backend_config.config]
}

resource "vault_policy" "app" {
  name   = "howling-app"
  policy = file("${path.module}/policies/howling-app.hcl")
}

resource "vault_kubernetes_auth_backend_role" "app" {
  backend        = vault_auth_backend.kubernetes.path
  role_name      = "howling-app-role"
  token_policies = [vault_policy.app.name]
  token_ttl      = 3600

  bound_service_account_names      = ["*"]
  bound_service_account_namespaces = ["howling-app"]

  depends_on = [vault_kubernetes_auth_backend_config.config]
}
