# ------------------------------------------------------------------------------
# Namespaces for ARC
# ------------------------------------------------------------------------------

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

# ------------------------------------------------------------------------------
# GitHub App Authentication Secret
# ------------------------------------------------------------------------------

resource "kubernetes_secret" "github_app_creds" {
  metadata {
    name      = "github-app-creds"
    namespace = kubernetes_namespace.arc_runners.metadata[0].name
  }

  type = "Opaque"

  data = {
    github_app_id              = var.github_app_id
    github_app_installation_id = var.github_app_installation_id
    github_app_private_key     = var.github_app_private_key
  }
}

# ------------------------------------------------------------------------------
# Actions Runner Controller (Controller)
# ------------------------------------------------------------------------------

resource "helm_release" "arc_controller" {
  name       = "arc-controller"
  repository = "oci://ghcr.io/actions/actions-runner-controller-charts"
  chart      = "gha-runner-scale-set-controller"
  namespace  = kubernetes_namespace.arc_systems.metadata[0].name
  version    = "0.9.3"
}

# ------------------------------------------------------------------------------
# ARC Runner Scale Set
# ------------------------------------------------------------------------------

resource "helm_release" "arc_runner_set" {
  name       = "arc-runner-set"
  repository = "oci://ghcr.io/actions/actions-runner-controller-charts"
  chart      = "gha-runner-scale-set"
  namespace  = kubernetes_namespace.arc_runners.metadata[0].name
  version    = "0.9.3"

  set {
    name  = "githubConfigUrl"
    value = var.github_repo_url
  }

  # Reference the secret created above
  set {
    name  = "githubConfigSecret"
    value = kubernetes_secret.github_app_creds.metadata[0].name
  }

  set {
    name  = "minRunners"
    value = "0"
  }

  set {
    name  = "maxRunners"
    value = "3"
  }

  set {
    name  = "controllerServiceAccount.name"
    value = "arc-controller-gha-runner-scale-set-controller"
  }

  set {
    name  = "controllerServiceAccount.namespace"
    value = kubernetes_namespace.arc_systems.metadata[0].name
  }

  depends_on = [helm_release.arc_controller]
}

# ------------------------------------------------------------------------------
# Runner Identity (ServiceAccount for Vault Auth)
# ------------------------------------------------------------------------------

resource "kubernetes_service_account" "ci_runner" {
  metadata {
    name      = "howling-ci-runner"
    namespace = kubernetes_namespace.arc_runners.metadata[0].name
  }
}
