#include "services/security/register.h"

#include <chrono>
#include <memory>

#include "absl/flags/flag.h"
#include "absl/log/log.h"
#include "absl/time/time.h"
#include "services/registry/register_service.h"
#include "services/security.h"
#include "services/security/bao_client.h"
#include "time/conversion.h"

ABSL_FLAG(
    absl::Duration,
    bao_startup_timeout,
    absl::Minutes(10),
    "How long to wait for Bao to connect at service start.");

namespace howling::security {

void register_security_client() {
  registry::register_service_factory([]() -> std::unique_ptr<security_client> {
    auto bao = std::make_unique<bao_client>();
    LOG(INFO) << "Waiting for bao to be available.";
    bao->wait_for_ready(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            to_std_chrono(absl::GetFlag(FLAGS_bao_startup_timeout))));
    return bao;
  });
}

} // namespace howling::security
