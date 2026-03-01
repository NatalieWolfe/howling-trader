#include "services/oauth/notification.h"

#include <format>
#include <string>
#include <string_view>

#include "absl/flags/flag.h"
#include "absl/log/log.h"
#include "boost/beast/core.hpp"
#include "boost/beast/http.hpp"
#include "boost/beast/version.hpp"
#include "net/connect.h"
#include "net/url.h"
#include "strings/format.h"
#include "strings/json.h"

ABSL_FLAG(
    std::string,
    telegram_bot_token,
    "",
    "Telegram bot token for notifications.");
ABSL_FLAG(
    std::string, telegram_chat_id, "", "Telegram chat ID for notifications.");
ABSL_FLAG(std::string, telegram_host, "api.telegram.org", "Telegram API host.");
ABSL_FLAG(uint16_t, telegram_port, 443, "Telegram API port.");

namespace howling::oauth {
namespace {

namespace beast = ::boost::beast;
namespace http = ::boost::beast::http;

constexpr std::string_view MESSAGE_MODE = "MarkdownV2";

} // namespace

void send_notification(std::string_view message) {
  std::string token = absl::GetFlag(FLAGS_telegram_bot_token);
  std::string chat_id = absl::GetFlag(FLAGS_telegram_chat_id);
  std::string host = absl::GetFlag(FLAGS_telegram_host);

  if (token.empty() || chat_id.empty()) {
    throw std::runtime_error(
        "Telegram configuration (bot_token/chat_id) missing.");
  }

  try {
    const std::string target = std::format("/bot{}/sendMessage", token);
    auto conn = net::make_connection(
        net::url{
            .service = std::to_string(absl::GetFlag(FLAGS_telegram_port)),
            .host = host,
            .target = target});

    Json::Value body;
    body["chat_id"] = std::move(chat_id);
    body["text"] = escape_markdown_v2(message);
    body["parse_mode"] = std::string{MESSAGE_MODE};

    std::string body_str = to_string(body);

    http::request<http::string_body> req{http::verb::post, target, 11};
    req.set(http::field::host, host);
    req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);
    req.set(http::field::content_type, "application/json");
    req.set(http::field::content_length, std::to_string(body_str.size()));
    req.body() = std::move(body_str);

    http::write(conn->stream(), req);

    beast::flat_buffer buffer;
    http::response<http::string_body> res;
    http::read(conn->stream(), buffer, res);

    if (res.result() != http::status::ok) {
      throw std::runtime_error(
          std::format(
              "Telegram API returned error {}: {}",
              static_cast<int>(res.result()),
              res.body()));
    }

    LOG(INFO) << "Telegram notification sent successfully.";

  } catch (const std::exception& e) {
    LOG(ERROR) << "Failed to send Telegram notification: " << e.what();
    throw;
  }
}

} // namespace howling::oauth
