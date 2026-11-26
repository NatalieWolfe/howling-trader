#pragma once

namespace howling {

/**
 * @brief Registers reader_db and reader_db_pool in the service registry using
 * the Postgres flags declared in services/db/environment.h.
 */
void register_reader_db();

} // namespace howling
