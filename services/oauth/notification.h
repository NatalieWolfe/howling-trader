#pragma once

#include <string_view>

namespace howling::oauth {

/**
 * @brief Checks that all required Telegram notification flags are set.
 *
 * @throws std::runtime_error if any required flags are empty.
 */
void check_telegram_flags();

/**
 * @brief Sends the given message as a notification to the user.
 */
void send_notification(std::string_view message);

} // namespace howling::oauth
