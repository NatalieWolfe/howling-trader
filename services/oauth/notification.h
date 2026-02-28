#pragma once

#include <string_view>

namespace howling::oauth {

/**
 * @brief Sends a notification via Telegram using configured bot_token and chat_id flags.
 */
void send_notification(std::string_view message);

} // namespace howling::oauth
