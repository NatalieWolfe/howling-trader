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
}
