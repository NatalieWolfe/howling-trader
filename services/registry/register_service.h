#pragma once

#include <functional>
#include <memory>
#include <typeindex>

#include "services/registry/registry.h"
#include "services/service_base.h"

namespace howling {
namespace services_registry_internal {

void store_service(
    std::type_index service_type_id, std::unique_ptr<service> service_ptr);
void store_service_factory(
    std::type_index service_type_id,
    std::function<std::unique_ptr<service>()> factory);

} // namespace services_registry_internal

namespace registry {

template <typename Service, typename... Dependencies>
void register_service_factory(
    std::function<std::unique_ptr<Service>(Dependencies...)> factory) {
  services_registry_internal::store_service_factory(
      std::type_index{typeid(Service)},
      [factory = std::move(factory)]() -> std::unique_ptr<service> {
        return factory(get_service<Dependencies>()...);
      });
}

template <typename Factory>
void register_service_factory(Factory&& factory) {
  register_service_factory(std::function{std::forward<Factory>(factory)});
}

template <typename Service>
void register_service(std::unique_ptr<Service> service) {
  services_registry_internal::store_service(
      std::type_index{typeid(Service)}, std::move(service));
}

} // namespace registry

} // namespace howling
