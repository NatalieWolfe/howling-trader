#include <exception>
#include <format>
#include <functional>
#include <memory>
#include <tuple>
#include <typeindex>
#include <unordered_map>

#include "services/service_base.h"

namespace howling::services_registry_internal {
namespace {

using cached_service_t = std::unique_ptr<service>;
using service_factory_t = std::function<cached_service_t()>;
using factory_map_t = std::unordered_map<std::type_index, service_factory_t>;
using service_cache_map_t =
    std::unordered_map<std::type_index, cached_service_t>;

factory_map_t& get_factory_map() {
  static factory_map_t factory_map;
  return factory_map;
}

service_cache_map_t& get_service_cache_map() {
  static service_cache_map_t service_cache_map;
  return service_cache_map;
}

} // namespace

service& retrieve_service(std::type_index service_type_id) {
  service_cache_map_t& service_cache = get_service_cache_map();
  auto cache_itr = service_cache.find(service_type_id);
  if (cache_itr == service_cache.end()) {
    factory_map_t& factory_map = get_factory_map();
    auto factory_itr = factory_map.find(service_type_id);
    if (factory_itr == factory_map.end()) {
      throw std::runtime_error(
          std::format(
              "No factory registered for service {}", service_type_id.name()));
    }
    cache_itr =
        service_cache.emplace(service_type_id, factory_itr->second()).first;
  }
  return *cache_itr->second;
}

void store_service(
    std::type_index service_type_id, std::unique_ptr<service> service_ptr) {
  service_cache_map_t& service_cache = get_service_cache_map();
  auto cache_itr = service_cache.find(service_type_id);
  if (cache_itr != service_cache.end()) {
    throw std::runtime_error(
        std::format(
            "Duplicate registration for service {}", service_type_id.name()));
  }
  service_cache.emplace(service_type_id, std::move(service_ptr));
}

void store_service_factory(
    std::type_index service_type_id, service_factory_t factory) {
  factory_map_t& factory_map = get_factory_map();
  auto factory_itr = factory_map.find(service_type_id);
  if (factory_itr != factory_map.end()) {
    throw std::runtime_error(
        std::format(
            "Duplicate factory registered for service {}",
            service_type_id.name()));
  }
  factory_map.emplace(service_type_id, factory);
}

} // namespace howling::services_registry_internal

namespace howling {

void clear_service_registry_cache() {
  services_registry_internal::get_factory_map().clear();
  services_registry_internal::get_service_cache_map().clear();
}

} // namespace howling
