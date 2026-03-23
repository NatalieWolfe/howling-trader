#include "services/registry/registry.h"

#include <exception>
#include <memory>

#include "services/registry/register_service.h"
#include "services/service_base.h"
#include "gtest/gtest.h"

namespace howling::registry {
namespace {

TEST(Registry, CanRegisterFactories) {
  struct can_register_factories_t : public service {};
  EXPECT_NO_THROW(register_service_factory(
      []() { return std::make_unique<can_register_factories_t>(); }));
}

TEST(Registry, CanRegisterServices) {
  struct can_register_services_t : public service {};
  EXPECT_NO_THROW(
      register_service(std::make_unique<can_register_services_t>()));
}

TEST(Registry, ThrowsWhenRetrievingServiceWithoutFactory) {
  struct service_without_factory_t : public service {};
  EXPECT_THROW(get_service<service_without_factory_t>(), std::exception);
}

TEST(Registry, CallsServiceFactory) {
  struct service_t : public service {};
  int called = 0;
  register_service_factory([&]() {
    ++called;
    return std::make_unique<service_t>();
  });
  EXPECT_EQ(called, 0);
  get_service<service_t>();
  EXPECT_EQ(called, 1);

  // Factory only called the first time.
  get_service<service_t>();
  get_service<service_t>();
  get_service<service_t>();
  EXPECT_EQ(called, 1);
}

TEST(Registry, DependenciesImplicitlyBuilt) {
  struct dep_t : public service {
    int val = 42;
  };
  struct service_t : public service {
    explicit service_t(int val) : val_from_dep{val} {}
    int val_from_dep;
  };

  register_service_factory([](dep_t& dep) {
    return std::make_unique<service_t>(service_t(dep.val));
  });
  register_service_factory([]() { return std::make_unique<dep_t>(); });

  EXPECT_EQ(get_service<service_t>().val_from_dep, 42);
}

TEST(Registry, CanRetrieveDirectlyInjectedServices) {
  struct directly_injected_service_t : public service {};
  register_service(std::make_unique<directly_injected_service_t>());
  EXPECT_NO_THROW(get_service<directly_injected_service_t>());
}

} // namespace
} // namespace howling::registry
