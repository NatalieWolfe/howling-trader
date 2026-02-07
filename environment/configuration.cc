#include "environment/configuration.h"

#include "absl/flags/flag.h"

ABSL_FLAG(bool, use_real_money, false, "Enables real money trading.");
ABSL_FLAG(
    bool, headless, false, "Disables interactive UI and terminal printing.");
