# Application Secret Management
path "secret/data/howling/*" {
  capabilities = ["create", "read", "update", "delete", "list"]
}

path "secret/metadata/howling/*" {
  capabilities = ["create", "read", "update", "delete", "list"]
}

path "secret/delete/howling/*" {
  capabilities = ["update"]
}

path "secret/undelete/howling/*" {
  capabilities = ["update"]
}

path "secret/destroy/howling/*" {
  capabilities = ["update"]
}

# Transit Management
path "transit/keys/*" {
  capabilities = ["create", "read", "update", "delete", "list"]
}

# Session Management (Ephemeral tokens for Tofu)
path "auth/token/create" {
  capabilities = ["update"]
}

# Discover and Manage Auth Methods and Mounts
path "sys/auth" {
  capabilities = ["read", "list"]
}

path "sys/auth/kubernetes" {
  capabilities = ["create", "read", "update", "delete", "sudo"]
}

path "sys/mounts" {
  capabilities = ["read", "list"]
}

path "sys/mounts/secret" {
  capabilities = ["create", "read", "update", "delete", "sudo"]
}

path "sys/mounts/transit" {
  capabilities = ["create", "read", "update", "delete", "sudo"]
}

path "sys/mounts/pki" {
  capabilities = ["create", "read", "update", "delete", "sudo"]
}

# PKI Management
path "pki/*" {
  capabilities = ["create", "read", "update", "delete", "list", "sudo"]
}

# Configure Kubernetes Backend
path "auth/kubernetes/config" {
  capabilities = ["create", "read", "update", "delete"]
}

path "auth/kubernetes/role" {
  capabilities = ["list"]
}

path "auth/kubernetes/role/*" {
  capabilities = ["create", "read", "update", "delete", "list"]
}

# MARK: ACLs

path "sys/policies/acl" {
  capabilities = ["list"]
}

path "sys/policies/acl/howling-ci-app" {
  capabilities = ["create", "read", "update", "delete", "list", "sudo"]
}

path "sys/policies/acl/howling-app" {
  capabilities = ["create", "read", "update", "delete", "list", "sudo"]
}

path "sys/policies/acl/monitoring" {
  capabilities = ["create", "read", "update", "delete", "list", "sudo"]
}

path "identity/oidc/*" {
  capabilities = ["create", "read", "update", "delete", "list"]
}
