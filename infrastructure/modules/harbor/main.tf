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
