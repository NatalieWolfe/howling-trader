locals {
  registry             = "${var.registry_server}/${var.registry_name}"
  service_account_name = kubernetes_service_account.trader.metadata[0].name
}

resource "kubernetes_service_account" "trader" {
  metadata {
    name      = "howling-trader"
    namespace = var.namespace
  }
}

resource "kubernetes_cron_job_v1" "howling_trader" {
  metadata {
    name      = "howling-trader"
    namespace = var.namespace
    labels = {
      app = "howling-trader"
    }
  }
  spec {
    schedule           = "0 9 * * 1-5" # Monday - Friday at 9am Eastern.
    timezone           = "America/New_York"
    concurrency_policy = "Forbid"

    successful_jobs_history_limit = 7
    failed_jobs_history_limit     = 7

    job_template {
      metadata {}
      spec {
        backoff_limit              = 10
        ttl_seconds_after_finished = 3600 * 24 # 24 hours in seconds
        active_deadline_seconds    = 3600 * 10 # 10 hours in seconds

        template {
          metadata {
            labels = {
              app = "howling-trader"
            }
            annotations = {
              "vault.hashicorp.com/role"         = "howling-app-role"
              "vault.hashicorp.com/agent-inject" = "true"
              "vault.hashicorp.com/agent-image"  = var.openbao_agent_image

              "vault.hashicorp.com/agent-cache-enable"              = "true"
              "vault.hashicorp.com/agent-cache-use-auto-auth-token" = "force"
            }
          }
          spec {
            service_account_name = local.service_account_name
            image_pull_secrets {
              name = var.registry_credentials
            }

            volume {
              name = "tmp-howling"
              empty_dir { medium = "Memory" }
            }

            container {
              name  = "howling-trader"
              image = "${local.registry}/${var.executor_image_repository}:${var.image_tag}"

              volume_mount {
                name       = "tmp-howling"
                mount_path = "/tmp/howling"
              }

              args = [
                # General flags:
                "--auth_service_address=${var.auth_service_address}",
                "--db_encryption_key_name=${var.db_encryption_key_name}",
                "--database=postgres",
                "--pg_host=${var.db_host}",
                "--pg_port=${var.db_port}",
                "--pg_database=howling",
                "--pg_enable_encryption",
                "--logging_mode=json",
                # Execute flags:
                "--stocks=AAPL,AMD,MU,NVDA,RIVN",
                "--account=620",
                "--headless",
              ]

              resources {
                # TODO: Add monitoring on resource usages for all services and
                # adjust resource limits/requests accordingly.
                limits   = { cpu = "1000m", memory = "1024Mi" }
                requests = { cpu = "500m", memory = "512Mi" }
              }

              startup_probe {
                exec {
                  command = [
                    "/bin/sh",
                    "-c",
                    join(" ", [
                      "test -f /tmp/howling/candle-beat &&",
                      "test -f /tmp/howling/market-beat"
                    ])
                  ]
                }
                initial_delay_seconds = 10
                period_seconds        = 10
                failure_threshold     = 60 # 10 minutes to start up (60 * 10s).
              }

              liveness_probe {
                exec {
                  command = [
                    "/bin/sh",
                    "-c",
                    join(" ", [
                      "find /tmp/howling/candle-beat -mmin -2 | grep -q . &&",
                      "find /tmp/howling/market-beat -mmin -2 | grep -q ."
                    ])
                  ]
                }
                period_seconds    = 15
                failure_threshold = 13 # 120s (mmin 2) + (12 * 15s) = 5 minutes.
              }
            }

            restart_policy = "OnFailure"
          }
        }
      }
    }
  }
}
