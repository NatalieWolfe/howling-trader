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
# 1. Application Secret Management
path "secret/data/howling/*" {
  capabilities = ["create", "read", "update", "delete", "list"]
}

path "secret/metadata/howling/*" {
  capabilities = ["create", "read", "update", "delete", "list"]
}

path "secret/delete/howling/*" {
  capabilities = ["update"]
}

path "secret/undelete/howling/*" {
  capabilities = ["update"]
}

path "secret/destroy/howling/*" {
  capabilities = ["update"]
}

# 2. Session Management (Ephemeral tokens for Tofu)
path "auth/token/create" {
  capabilities = ["update"]
}

# 3. Discover and Manage Auth Methods and Mounts
path "sys/auth" {
  capabilities = ["read", "list"]
}

path "sys/auth/kubernetes" {
  capabilities = ["create", "read", "update", "delete", "sudo"]
}

path "sys/mounts" {
  capabilities = ["read", "list"]
}

path "sys/mounts/secret" {
  capabilities = ["create", "read", "update", "delete", "sudo"]
}

# 4. Configure Kubernetes Backend
path "auth/kubernetes/config" {
  capabilities = ["create", "read", "update", "delete"]
}

path "auth/kubernetes/role" {
  capabilities = ["list"]
}

path "auth/kubernetes/role/*" {
  capabilities = ["create", "read", "update", "delete", "list"]
}

# 5. Manage Policies (Self-Management and App Policies)
path "sys/policies/acl" {
  capabilities = ["list"]
}

path "sys/policies/acl/howling-ci-app" {
  capabilities = ["create", "read", "update", "delete", "list", "sudo"]
}

path "sys/policies/acl/howling-app" {
  capabilities = ["create", "read", "update", "delete", "list", "sudo"]
}
EOT
}

resource "vault_kubernetes_auth_backend_role" "ci_runner" {
  backend                          = vault_auth_backend.kubernetes.path
  role_name                        = "howling-ci-role"
  bound_service_account_names      = ["*"]
  bound_service_account_namespaces = [var.runner_namespace, "howling-admin"]
  token_policies                   = [vault_policy.ci_app.name]
  token_ttl                        = 3600

  depends_on = [vault_kubernetes_auth_backend_config.config]
}

resource "vault_policy" "app" {
  name   = "howling-app"
  policy = <<EOT
# 1. Read-only access to application secrets (Runtime only)
path "secret/data/howling/prod/*" {
  capabilities = ["read", "list"]
}

# 2. Access to Transit engine for database encryption/decryption
path "transit/encrypt/howling-db-key" {
  capabilities = ["update"]
}

path "transit/decrypt/howling-db-key" {
  capabilities = ["update"]
}
EOT
}

resource "vault_kubernetes_auth_backend_role" "app" {
  backend                          = vault_auth_backend.kubernetes.path
  role_name                        = "howling-app-role"
  bound_service_account_names      = ["*"]
  bound_service_account_namespaces = ["howling-app"]
  token_policies                   = [vault_policy.app.name]
  token_ttl                        = 3600

  depends_on = [vault_kubernetes_auth_backend_config.config]
}
