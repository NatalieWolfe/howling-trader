#include "services/db/environment.h"

#include <string>

#include "absl/flags/flag.h"

ABSL_FLAG(
    std::string,
    db_encryption_key_name,
    "",
    "The name of the encryption key to be used by the database.");
