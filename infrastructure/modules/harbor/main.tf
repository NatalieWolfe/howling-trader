resource "harbor_project" "main_project" {
  name                   = var.registry_name
  public                 = false
  vulnerability_scanning = true
}
