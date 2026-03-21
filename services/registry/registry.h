#pragma once

#include <typeindex>

#include "services/service_base.h"

namespace howling {
namespace services_registry_internal {

service& retrieve_service(std::type_index service_type_id);

} // namespace services_registry_internal

namespace registry {

template <typename Service>
Service& get_service() {
  return static_cast<Service&>(services_registry_internal::retrieve_service(
      std::type_index{typeid(Service)}));
}

} // namespace registry

} // namespace howling
