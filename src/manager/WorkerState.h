//
// Created by user on 3/4/25.
//

#ifndef WORKERSTATE_H
#define WORKERSTATE_H
#include "boost/uuid/uuid.hpp"
#include "userver/clients/http/client.hpp"

#include <task/Md5PartSolver.h>
#include "ManagerTask.h"

namespace manager {

class WorkerState {
public:
  using WorkerTaskId = boost::uuids::uuid;

  enum WorkerStatus{DONE, IN_PROGRESS, DEAD};

  WorkerState(const std::string& host, std::uint16_t port, userver::clients::http::Client& client);

  bool addTask(const WorkerTask& task);

  void cancelTask();

  void updateStatus();

  [[nodiscard]] WorkerStatus getLastWorkerStatus() const { return lastWorkerStatus;}
  [[nodiscard]] std::unique_ptr<std::string>& getLastTaskResult() {return lastTaskResult;}
  [[nodiscard]] std::unique_ptr<WorkerTask>& getWorkerTask() {return workerTask;}
private:
  std::unique_ptr<WorkerTaskId> workerTaskId;
  std::unique_ptr<WorkerTask> workerTask;
  std::string urlBase;
  userver::clients::http::Client& httpClient;
  std::unique_ptr<std::string> lastTaskResult;
  WorkerStatus lastWorkerStatus;

};

} // manager

#endif //WORKERSTATE_H
