# ------------------------------------------------------------------------------
# Service Account
# ------------------------------------------------------------------------------

resource "kubernetes_service_account" "oauth" {
  metadata {
    name      = "howling-oauth"
    namespace = var.namespace
  }
}

# ------------------------------------------------------------------------------
# Image Pull Secret
# ------------------------------------------------------------------------------

resource "kubernetes_secret" "registry_creds" {
  metadata {
    name      = "harbor-registry-creds"
    namespace = var.namespace
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
    name      = "howling-oauth"
    namespace = var.namespace
    labels = {
      app            = "howling-oauth"
      "db-bootstrap" = var.db_bootstrap_job_name
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
        annotations = {
          "vault.hashicorp.com/agent-inject"                    = "true"
          "vault.hashicorp.com/role"                            = "howling-app-role"
          "vault.hashicorp.com/agent-cache-enable"              = "true"
          "vault.hashicorp.com/agent-cache-use-auto-auth-token" = "force"
          "vault.hashicorp.com/agent-image"                     = var.openbao_agent_image
        }
      }

      spec {
        service_account_name = kubernetes_service_account.oauth.metadata[0].name
        image_pull_secrets {
          name = kubernetes_secret.registry_creds.metadata[0].name
        }

        container {
          name  = "oauth-service"
          image = "${var.image_repository}:${var.image_tag}"

          args = [
            "--db_encryption_key_name=${var.db_encryption_key_name}",
            "--database=postgres",
            "--pg_host=${var.db_host}",
            "--pg_port=${var.db_port}",
            "--pg_database=howling",
            "--pg_enable_encryption",
            "--logging_mode=json",
          ]

          port {
            container_port = 8080 # HTTP
            name           = "http"
          }

          port {
            container_port = 50051 # gRPC
            name           = "grpc"
          }

          readiness_probe {
            http_get {
              path = "/status"
              port = "http"
            }
            initial_delay_seconds = 5
            period_seconds        = 10
          }

          liveness_probe {
            http_get {
              path = "/status"
              port = "http"
            }
            initial_delay_seconds = 15
            period_seconds        = 20
          }
        }
      }
    }
  }
}

resource "kubernetes_service" "oauth" {
  metadata {
    name      = "howling-oauth"
    namespace = var.namespace
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
    name      = "howling-oauth"
    namespace = var.namespace
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
}
