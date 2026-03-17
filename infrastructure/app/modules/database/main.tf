resource "ovh_cloud_project_database" "postgres" {
  service_name = var.service_name
  description  = var.db_name
  engine       = var.db_engine
  version      = var.db_version
  plan         = var.db_plan
  flavor       = var.db_flavor

  nodes {
    region     = var.region
    network_id = var.network_id
    subnet_id  = var.subnet_id
  }

  dynamic "ip_restrictions" {
    for_each = var.authorized_subnets
    content {
      ip          = ip_restrictions.value
      description = "Authorized subnet"
    }
  }
}

resource "ovh_cloud_project_database_database" "main" {
  service_name = var.service_name
  engine       = var.db_engine
  cluster_id   = ovh_cloud_project_database.postgres.id
  name         = "howling"
}

resource "ovh_cloud_project_database_postgresql_user" "app_user" {
  service_name = var.service_name
  cluster_id   = ovh_cloud_project_database.postgres.id
  name         = "howling_app"
}

resource "ovh_cloud_project_database_postgresql_user" "admin" {
  service_name = var.service_name
  cluster_id   = ovh_cloud_project_database.postgres.id
  name         = "avnadmin"
}

# MARK: DB Bootstrap

resource "kubernetes_secret" "registry_creds" {
  metadata {
    name      = "harbor-registry-creds-db"
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

resource "kubernetes_job" "db_bootstrap" {
  metadata {
    name      = "howling-db-bootstrap"
    namespace = var.namespace
  }

  spec {
    template {
      metadata {
        name = "howling-db-bootstrap"
        annotations = {
          "vault.hashicorp.com/agent-inject"                    = "true"
          "vault.hashicorp.com/role"                            = "howling-ci-role"
          "vault.hashicorp.com/agent-cache-enable"              = "true"
          "vault.hashicorp.com/agent-cache-use-auto-auth-token" = "force"
          "vault.hashicorp.com/agent-image"                     = var.openbao_agent_image
        }
      }
      spec {
        image_pull_secrets {
          name = kubernetes_secret.registry_creds.metadata[0].name
        }

        container {
          name  = "bootstrap"
          image = "${var.image_repository}:${var.image_tag}"
          args = [
            "--database=postgres",
            "--pg_host=${ovh_cloud_project_database.postgres.endpoints[0].domain}",
            "--pg_port=${ovh_cloud_project_database.postgres.endpoints[0].port}",
            "--pg_database=howling",
            "--pg_enable_encryption=true",
            "--logging_mode=json",
          ]
        }
        restart_policy = "OnFailure"
      }
    }
    backoff_limit = 4
  }

  wait_for_completion = true

  timeouts {
    create = "2m"
    update = "2m"
  }
}
