
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
