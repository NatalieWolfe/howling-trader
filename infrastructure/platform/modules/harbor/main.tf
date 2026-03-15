resource "harbor_project" "main_project" {
  name                   = var.registry_name
  public                 = false
  vulnerability_scanning = true
}

resource "harbor_retention_policy" "main_retention" {
  scope    = harbor_project.main_project.id
  schedule = "Daily"

  rule {
    disabled             = false
    most_recently_pushed = 5
    repo_matching        = "{**}"
    tag_matching         = "{*}"
    untagged_artifacts   = true
  }
}

resource "harbor_garbage_collection" "main_gc" {
  schedule        = "Daily"
  delete_untagged = true
}

# MARK: Image Mirroring

resource "harbor_registry" "ghcr" {
  provider_name = "docker-registry"
  name          = "ghcr"
  endpoint_url  = "https://ghcr.io"
  access_id     = var.github_username
  access_secret = var.ghcr_readonly_token
}

resource "harbor_replication" "pull_openbao_agent" {
  name        = "pull-openbao-agent"
  action      = "pull"
  description = "Mirror OpenBao Agent sidecar image from GHCR"

  registry_id    = harbor_registry.ghcr.registry_id
  dest_namespace = harbor_project.main_project.name

  filters {
    name = "openbao/openbao-agent"
    tag  = "latest"
  }

  schedule           = "0 0 0 * * 6" # Weekly on Saturday at midnight
  execute_on_changed = true
}
