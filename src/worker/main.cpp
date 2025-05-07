#include <userver/clients/dns/component.hpp>
#include <userver/urabbitmq/component.hpp>
#include <userver/components/minimal_server_component_list.hpp>
#include <userver/storages/secdist/component.hpp>
#include <userver/storages/secdist/provider_component.hpp>
#include <userver/server/handlers/ping.hpp>
#include <userver/server/handlers/tests_control.hpp>
#include <userver/testsuite/testsuite_support.hpp>
#include <userver/utils/daemon_run.hpp>

#include "BackgroundTaskProcessor.h"
#include "WorkerHandlers.h"
int main(int argc, char* argv[]) {
  auto component_list = userver::components::MinimalServerComponentList()
                            .Append<userver::server::handlers::Ping>()
                            .Append<userver::clients::dns::Component>()
                            .Append<userver::components::DefaultSecdistProvider>()
                            .Append<userver::components::Secdist>()
                            .Append<userver::components::TestsuiteSupport>()
                            .Append<userver::server::handlers::TestsControl>()
                            .Append<userver::components::RabbitMQ>("rabbitmq")
                            .Append<worker::BackgroundTaskProcessor>();

  worker::AppendWorkerEndpoints(component_list);
  return userver::utils::DaemonMain(argc, argv, component_list);
}