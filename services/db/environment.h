#pragma once

#include <cstddef>
#include <string>

#include "absl/flags/declare.h"

ABSL_DECLARE_FLAG(std::string, db_encryption_key_name);
ABSL_DECLARE_FLAG(std::string, pg_user);
ABSL_DECLARE_FLAG(std::string, pg_password);
ABSL_DECLARE_FLAG(std::string, pg_host);
ABSL_DECLARE_FLAG(int, pg_port);
ABSL_DECLARE_FLAG(std::string, pg_database);
ABSL_DECLARE_FLAG(bool, pg_enable_encryption);
ABSL_DECLARE_FLAG(size_t, db_pool_min_size);
ABSL_DECLARE_FLAG(size_t, db_pool_max_size);
