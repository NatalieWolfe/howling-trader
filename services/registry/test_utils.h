#pragma once

namespace howling {

/**
 * @brief Deletes everything in the service registry caches.
 *
 * This includes all built services and all service factories.
 */
void clear_service_registry_cache();

} // namespace howling
