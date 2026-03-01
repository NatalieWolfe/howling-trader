#include "services/oauth/notification.h"

#include <chrono>
#include <memory>
#include <string>
#include <thread>

#include "absl/flags/declare.h"
#include "absl/flags/flag.h"
#include "services/oauth/mock_telegram_server.h"
#include "strings/json.h"
#include "gtest/gtest.h"

// We need these for the manual override test
ABSL_DECLARE_FLAG(std::string, telegram_bot_token);
ABSL_DECLARE_FLAG(std::string, telegram_chat_id);

namespace howling::oauth {
namespace {

TEST(NotificationTest, SendNotificationSendsCorrectSSLRequest) {
  mock_telegram_server server;
  server.start();

  EXPECT_NO_THROW(send_notification("Hello Telegram SSL!"));

  EXPECT_EQ(
      server.last_request_target(),
      std::format("/bot{}/sendMessage", mock_telegram_server::BOT_TOKEN));

  Json::Value body = to_json(server.last_request_body());
  EXPECT_EQ(body["chat_id"].asString(), mock_telegram_server::CHAT_ID);
  EXPECT_EQ(body["text"].asString(), "Hello Telegram SSL\\!");
}

TEST(NotificationTest, ThrowsOnMissingConfig) {
  // Use mock server without auto-configuration to test missing flags
  mock_telegram_server server(/*configure_flags=*/false);

  absl::SetFlag(&FLAGS_telegram_bot_token, "");
  absl::SetFlag(&FLAGS_telegram_chat_id, "");

  EXPECT_THROW(send_notification("test"), std::runtime_error);
}

} // namespace
} // namespace howling::oauth
