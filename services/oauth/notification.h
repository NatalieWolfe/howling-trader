#pragma once

#include <string_view>

namespace howling::oauth {

/**
 * @brief Sends the given message as a notification to the user.
 */
void send_notification(std::string_view message);

} // namespace howling::oauth
