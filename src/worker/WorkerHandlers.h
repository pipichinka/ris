#ifndef WORKERHANDLERS_H
#define WORKERHANDLERS_H

#include <userver/components/component_list.hpp>
namespace worker {

void AppendWorkerEndpoints(userver::components::ComponentList &component_list);

}
#endif //WORKERHANDLERS_H
