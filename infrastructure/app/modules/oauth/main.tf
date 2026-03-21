locals {
  registry             = "${var.registry_server}/${var.registry_name}"
  registry_credentials = kubernetes_secret.registry_creds.metadata[0].name
  auth_service_address = "${kubernetes_service.oauth.metadata[0].name}:50051"
  service_account_name = kubernetes_service_account.oauth.metadata[0].name
  bao_sidecar_annotations = {
    "vault.hashicorp.com/agent-inject"                    = "true"
    "vault.hashicorp.com/role"                            = "howling-app-role"
    "vault.hashicorp.com/agent-cache-enable"              = "true"
    "vault.hashicorp.com/agent-cache-use-auto-auth-token" = "force"
    "vault.hashicorp.com/agent-image"                     = var.openbao_agent_image
  }
  common_flags = [
    "--db_encryption_key_name=${var.db_encryption_key_name}",
    "--database=postgres",
    "--pg_host=${var.db_host}",
    "--pg_port=${var.db_port}",
    "--pg_database=howling",
    "--pg_enable_encryption",
    "--logging_mode=json",
  ]
}

# MARK: Service Account

resource "kubernetes_service_account" "oauth" {
  metadata {
    name      = "howling-oauth"
    namespace = var.namespace
  }
}

# MARK: Registry Image

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

# MARK: OAuth Service

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
        annotations = local.bao_sidecar_annotations
      }

      spec {
        service_account_name = local.service_account_name
        image_pull_secrets {
          name = local.registry_credentials
        }

        container {
          name  = "oauth-service"
          image = "${local.registry}/${var.oauth_service_repository}:${var.image_tag}"

          args = local.common_flags

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

# MARK: HTTPS Ingress

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

# MARK: Auth Refresh Job

resource "kubernetes_cron_job_v1" "auth_refresh" {
  metadata {
    name      = "auth-refresh"
    namespace = var.namespace
    labels = {
      app = "auth-refresh"
    }
  }
  spec {
    schedule                  = "0 17 * * 0-4" # Sunday - Thursday at 5pm Pacific.
    timezone                  = "America/Los_Angeles"
    concurrency_policy        = "Forbid"
    failed_jobs_history_limit = 5

    job_template {
      metadata {}
      spec {
        backoff_limit              = 10
        ttl_seconds_after_finished = 3600 * 12 # 12 hours in seconds

        template {
          metadata {
            labels = {
              app = "auth-refresh"
            }
            annotations = local.bao_sidecar_annotations
          }
          spec {
            service_account_name = local.service_account_name
            image_pull_secrets {
              name = local.registry_credentials
            }

            container {
              name  = "auth-refresh"
              image = "${local.registry}/${var.auth_refresh_repository}:${var.image_tag}"

              args = concat(local.common_flags, [
                "--auth_service_address=${local.auth_service_address}",
              ])

              resources {
                # TODO: Add monitoring on resource usages for all services and
                # adjust resource limits/requests accordingly.
                limits   = { cpu = "250m", memory = "256Mi" }
                requests = { cpu = "100m", memory = "128Mi" }
              }
            }

            restart_policy = "OnFailure"
          }
        }
      }
    }
  }
}

# 4. Verification Plan (Part 3)
#  * Dry Run: Trigger the CronJob manually using
#    kubectl create job --from=cronjob/howling-auth-refresh.
#  * Log Verification: Check logs to confirm:
#      * OpenBao proxy starts and authenticates successfully.
#      * refresh_token is retrieved and decrypted.
#      * New token is saved to Postgres.
#  * Failure Test: Temporarily point --pg_host to a non-existent host and verify the gRPC call to howling-oauth is attempted and logged.
