resource "kubernetes_namespace" "monitoring" {
  metadata {
    name = "monitoring"
  }
}

data "vault_kv_secret_v2" "s3_credentials" {
  mount = "secret"
  name  = "howling/monitoring/s3"
}

data "vault_kv_secret_v2" "grafana_credentials" {
  mount = "secret"
  name  = "howling/monitoring/grafana"
}

locals {
  # Map human-readable regions to technical S3 region names.
  region_map = {
    "US-EAST-VA-1" = "us-east-va"
  }
  s3_region = lookup(local.region_map, var.region, lower(replace(var.region, "-1", "")))
}

resource "helm_release" "stack" {
  name              = "stack"
  chart             = "${path.module}/charts/stack"
  namespace         = kubernetes_namespace.monitoring.metadata[0].name
  dependency_update = true

  values = [
    templatefile("${path.module}/values.yaml.tftpl", {
      s3_endpoint = data.terraform_remote_state.platform.outputs.state_bucket_endpoint
      s3_bucket   = data.terraform_remote_state.platform.outputs.monitoring_bucket.name
      s3_region   = local.s3_region
      namespace   = kubernetes_namespace.monitoring.metadata[0].name
    })
  ]

  set_sensitive {
    name  = "loki.loki.storage.s3.accessKey"
    value = data.vault_kv_secret_v2.s3_credentials.data.access_key
  }

  set_sensitive {
    name  = "loki.loki.storage.s3.secretKey"
    value = data.vault_kv_secret_v2.s3_credentials.data.secret_key
  }

  set_sensitive {
    name  = "prometheus.grafana.adminPassword"
    value = data.vault_kv_secret_v2.grafana_credentials.data.admin_password
  }

  set_sensitive {
    name  = "prometheus.grafana.grafana\\.ini.auth\\.generic_oauth.client_id"
    value = data.vault_kv_secret_v2.grafana_credentials.data.client_id
  }

  set_sensitive {
    name  = "prometheus.grafana.grafana\\.ini.auth\\.generic_oauth.client_secret"
    value = data.vault_kv_secret_v2.grafana_credentials.data.client_secret
  }
}

resource "kubernetes_ingress_v1" "grafana" {
  metadata {
    name      = "grafana"
    namespace = kubernetes_namespace.monitoring.metadata[0].name
    annotations = {
      "cert-manager.io/cluster-issuer"           = "letsencrypt-prod"
      "nginx.ingress.kubernetes.io/ssl-redirect" = "true"
    }
  }

  spec {
    ingress_class_name = "nginx"

    tls {
      hosts       = ["howling-monitor.wolfe.dev"]
      secret_name = "howling-monitor-tls"
    }

    rule {
      host = "howling-monitor.wolfe.dev"
      http {
        path {
          path      = "/"
          path_type = "Prefix"
          backend {
            service {
              name = "grafana"
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
