resource "kubernetes_namespace" "monitoring" {
  metadata {
    name = "monitoring"
  }
}

data "vault_kv_secret_v2" "s3_creds" {
  mount = "secret"
  name  = "howling/monitoring/s3"
}

data "vault_kv_secret_v2" "grafana_creds" {
  mount = "secret"
  name  = "howling/monitoring/grafana"
}

resource "helm_release" "stack" {
  name       = "stack"
  chart      = "${path.module}/charts/stack"
  namespace  = kubernetes_namespace.monitoring.metadata[0].name
  depends_on = [kubernetes_namespace.monitoring]

  values = [
    templatefile("${path.module}/values.yaml.tftpl", {
      s3_endpoint = data.terraform_remote_state.platform.outputs.s3_endpoint
      s3_bucket   = data.terraform_remote_state.platform.outputs.monitoring_bucket.name
      s3_region   = lower(replace(var.region, "-1", ""))
      namespace   = kubernetes_namespace.monitoring.metadata[0].name
    })
  ]

  set_sensitive {
    name  = "loki.loki.storage.s3.access_key"
    value = data.vault_kv_secret_v2.s3_creds.data.access_key
  }

  set_sensitive {
    name  = "loki.loki.storage.s3.secret_key"
    value = data.vault_kv_secret_v2.s3_creds.data.secret_key
  }

  set_sensitive {
    name  = "prometheus.grafana.adminPassword"
    value = data.vault_kv_secret_v2.grafana_creds.data.admin_password
  }
}
