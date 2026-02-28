#include "services/oauth/notification.h"

#include <chrono>
#include <memory>
#include <string>
#include <thread>

#include "absl/flags/declare.h"
#include "absl/flags/flag.h"
#include "gtest/gtest.h"
#include "services/oauth/mock_telegram_server.h"
#include "strings/json.h"

ABSL_DECLARE_FLAG(std::string, telegram_bot_token);
ABSL_DECLARE_FLAG(std::string, telegram_chat_id);
ABSL_DECLARE_FLAG(std::string, telegram_host);
ABSL_DECLARE_FLAG(uint16_t, telegram_port);

namespace howling::oauth {
namespace {

TEST(NotificationTest, SendNotificationSendsCorrectSSLRequest) {
  mock_telegram_server server;
  server.start();

  absl::SetFlag(&FLAGS_telegram_bot_token, "test_token");
  absl::SetFlag(&FLAGS_telegram_chat_id, "test_chat");
  absl::SetFlag(&FLAGS_telegram_host, "127.0.0.1");
  absl::SetFlag(&FLAGS_telegram_port, server.port());

  const std::string message = "Hello Telegram SSL!";
  
  EXPECT_NO_THROW(send_notification(message));

  EXPECT_EQ(server.last_request_target(), "/bottest_token/sendMessage");
  
  Json::Value body = to_json(server.last_request_body());
  EXPECT_EQ(body["chat_id"].asString(), "test_chat");
  EXPECT_EQ(body["text"].asString(), message);
}

TEST(NotificationTest, ThrowsOnMissingConfig) {
  absl::SetFlag(&FLAGS_telegram_bot_token, "");
  absl::SetFlag(&FLAGS_telegram_chat_id, "");
  
  EXPECT_THROW(send_notification("test"), std::runtime_error);
}

} // namespace
} // namespace howling::oauth
