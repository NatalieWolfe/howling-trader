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
  policy = <<EOT
path "secret/data/howling/prod/*" {
  capabilities = ["create", "read", "update", "delete", "list"]
}
EOT
}

resource "vault_kubernetes_auth_backend_role" "ci_runner" {
  backend                          = vault_auth_backend.kubernetes.path
  role_name                        = "howling-ci-role"
  bound_service_account_names      = ["howling-ci-runner"]
  bound_service_account_namespaces = [var.runner_namespace]
  token_policies                   = [vault_policy.ci_app.name]
  token_ttl                        = 3600

  depends_on = [vault_kubernetes_auth_backend_config.config]
}
