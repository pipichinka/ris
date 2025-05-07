#include <userver/clients/dns/component.hpp>
#include <userver/clients/http/component.hpp>
#include <userver/components/minimal_server_component_list.hpp>
#include <userver/server/handlers/ping.hpp>
#include <userver/server/handlers/tests_control.hpp>
#include <userver/testsuite/testsuite_support.hpp>
#include <userver/utils/daemon_run.hpp>

#include "ManagerHandlers.h"
#include "Storage.h"
#include "TaskConsumer.h"
#include "userver/storages/mongo/component.hpp"
#include "userver/storages/secdist/provider_component.hpp"
#include "userver/urabbitmq/component.hpp"
int main(int argc, char* argv[]) {
  auto component_list = userver::components::MinimalServerComponentList()
                            .Append<userver::server::handlers::Ping>()
                            .Append<userver::components::TestsuiteSupport>()
                            .Append<userver::components::HttpClient>()
                            .Append<userver::clients::dns::Component>()
                            .Append<userver::components::DefaultSecdistProvider>()
                            .Append<userver::components::Secdist>()
                            .Append<userver::server::handlers::TestsControl>()
                            .Append<userver::components::Mongo>("mongo")
                            .Append<manager::Storage>()
                            .Append<userver::components::RabbitMQ>("rabbitmq");

  manager::AppendConsumers(component_list);
  manager::AppendMangerHandlers(component_list);
  return userver::utils::DaemonMain(argc, argv, component_list);
}