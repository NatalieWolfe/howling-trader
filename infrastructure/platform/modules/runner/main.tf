
locals {
  runner_namespace = kubernetes_namespace.arc_runners.metadata[0].name
  system_namespace = kubernetes_namespace.arc_systems.metadata[0].name
}

# MARK: Namespace

resource "kubernetes_namespace" "arc_systems" {
  metadata {
    name = "arc-systems"
  }
}

resource "kubernetes_namespace" "arc_runners" {
  metadata {
    name = "arc-runners"
  }
}

# MARK: Github Secrets

resource "kubernetes_secret" "github_app_creds" {
  metadata {
    name      = "github-app-creds"
    namespace = local.runner_namespace
  }

  type = "Opaque"

  data = {
    github_app_id              = var.github_app_id
    github_app_installation_id = var.github_app_installation_id
    github_app_private_key     = var.github_app_private_key
  }
}

# MARK: ARC Controller

resource "helm_release" "arc_controller" {
  name       = "arc-controller"
  repository = "oci://ghcr.io/actions/actions-runner-controller-charts"
  chart      = "gha-runner-scale-set-controller"
  namespace  = local.system_namespace
  version    = "0.9.3"

  set {
    name  = "nodeSelector.nodepool"
    value = var.system_pool_name
  }
}

# MARK: ARC Runner

resource "helm_release" "arc_runner_set" {
  name       = "arc-runner-set"
  repository = "oci://ghcr.io/actions/actions-runner-controller-charts"
  chart      = "gha-runner-scale-set"
  namespace  = local.runner_namespace
  version    = "0.9.3"

  values = [
    yamlencode({
      githubConfigUrl    = var.github_repo_url
      githubConfigSecret = kubernetes_secret.github_app_creds.metadata[0].name
      maxRunners         = 3
      minRunners         = 0
      controllerServiceAccount = {
        name      = "arc-controller-gha-rs-controller"
        namespace = local.system_namespace
      }
      template = {
        spec = {
          containers = [{
            name    = "runner"
            image   = "ghcr.io/actions/actions-runner:latest"
            command = ["/home/runner/run.sh"]
            resources = {
              requests = {
                cpu    = "7"
                memory = "24Gi"
              }
              limits = {
                cpu    = "7"
                memory = "24Gi"
              }
            }
          }]
          affinity = {
            nodeAffinity = {
              preferredDuringSchedulingIgnoredDuringExecution = [{
                weight = 100
                preference = {
                  matchExpressions = [{
                    key      = "nodepool"
                    operator = "In"
                    values   = [var.runner_pool_name]
                  }]
                }
              }]
            }
          }
        }
      }
    })
  ]

  depends_on = [helm_release.arc_controller]
}

# MARK: Service Account

resource "kubernetes_service_account" "ci_runner" {
  metadata {
    name      = "howling-ci-runner"
    namespace = local.runner_namespace
  }
}

resource "kubernetes_role" "arc_secret_reader" {
  metadata {
    name      = "arc-secret-reader"
    namespace = local.runner_namespace
  }
  rule {
    api_groups = [""]
    resources  = ["secrets"]
    verbs      = ["get", "list", "watch"]
  }
}
resource "kubernetes_role_binding" "arc_secret_reader_binding" {
  metadata {
    name      = "arc-secret-reader-binding"
    namespace = local.runner_namespace
  }
  role_ref {
    api_group = "rbac.authorization.k8s.io"
    kind      = "Role"
    name      = kubernetes_role.arc_secret_reader.metadata[0].name
  }
  subject {
    kind      = "ServiceAccount"
    name      = "arc-controller-gha-rs-controller"
    namespace = local.system_namespace
  }
}
