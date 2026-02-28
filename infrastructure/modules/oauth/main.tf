# ------------------------------------------------------------------------------
# Ingress Controller (Nginx)
# ------------------------------------------------------------------------------

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

# ------------------------------------------------------------------------------
# Cert-Manager
# ------------------------------------------------------------------------------

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

# ------------------------------------------------------------------------------
# Let's Encrypt ClusterIssuer
# ------------------------------------------------------------------------------

resource "kubernetes_manifest" "letsencrypt_issuer" {
  manifest = {
    apiVersion = "cert-manager.io/v1"
    kind       = "ClusterIssuer"
    metadata = {
      name = "letsencrypt-prod"
    }
    spec = {
      acme = {
        server = "https://acme-v02.api.letsencrypt.org/directory"
        email  = var.letsencrypt_email
        privateKeySecretRef = {
          name = "letsencrypt-prod"
        }
        solvers = [
          {
            http01 = {
              ingress = {
                class = "nginx"
              }
            }
          }
        ]
      }
    }
  }

  depends_on = [helm_release.cert_manager]
}

# ------------------------------------------------------------------------------
# Image Pull Secret
# ------------------------------------------------------------------------------

resource "kubernetes_secret" "registry_creds" {
  metadata {
    name = "harbor-registry-creds"
  }

  type = "kubernetes.io/dockerconfigjson"

  data = {
    ".dockerconfigjson" = jsonencode({
      auths = {
        (var.registry_server) = {
          auth = base64encode("${var.registry_username}:${var.registry_password}")
        }
      }
    })
  }
}

# ------------------------------------------------------------------------------
# OAuth Service Deployment
# ------------------------------------------------------------------------------

resource "kubernetes_deployment" "oauth" {
  metadata {
    name = "howling-oauth"
    labels = {
      app = "howling-oauth"
    }
  }

  spec {
    replicas = 2
    selector {
      match_labels = {
        app = "howling-oauth"
      }
    }

    template {
      metadata {
        labels = {
          app = "howling-oauth"
        }
      }

      spec {
        image_pull_secrets {
          name = kubernetes_secret.registry_creds.metadata[0].name
        }

        container {
          name  = "oauth-service"
          image = "${var.image_repository}:${var.image_tag}"

          port {
            container_port = 8080 # HTTP
            name           = "http"
          }

          port {
            container_port = 50051 # gRPC
            name           = "grpc"
          }

          env {
            name  = "DATABASE_URL"
            value = var.db_uri
          }
        }
      }
    }
  }
}

resource "kubernetes_service" "oauth" {
  metadata {
    name = "howling-oauth"
  }

  spec {
    selector = {
      app = "howling-oauth"
    }

    port {
      name        = "http"
      port        = 80
      target_port = "http"
    }

    port {
      name        = "grpc"
      port        = 50051
      target_port = "grpc"
    }

    type = "ClusterIP"
  }
}

# ------------------------------------------------------------------------------
# Ingress with HTTPS
# ------------------------------------------------------------------------------

resource "kubernetes_ingress_v1" "oauth" {
  metadata {
    name = "howling-oauth"
    annotations = {
      "kubernetes.io/ingress.class"              = "nginx"
      "cert-manager.io/cluster-issuer"           = "letsencrypt-prod"
      "nginx.ingress.kubernetes.io/ssl-redirect" = "true"
    }
  }

  spec {
    tls {
      hosts       = [var.domain_name]
      secret_name = "howling-oauth-tls"
    }

    rule {
      host = var.domain_name
      http {
        path {
          path      = "/"
          path_type = "Prefix"
          backend {
            service {
              name = kubernetes_service.oauth.metadata[0].name
              port {
                number = 80
              }
            }
          }
        }
      }
    }
  }

  depends_on = [kubernetes_manifest.letsencrypt_issuer]
}
