//
// Created by user on 3/4/25.
//

#include "ManagerState.h"

#include <worker/WorkerHandlers.h>

#include <boost/iostreams/filter/zlib.hpp>

#include "userver/cache/caching_component_base.hpp"
#include "userver/clients/http/component.hpp"
#include "userver/components/component_config.hpp"
#include "userver/components/component_context.hpp"
#include "userver/engine/sleep.hpp"
#include "userver/utils/async.hpp"
#include "userver/yaml_config/merge_schemas.hpp"

namespace manager {

userver::yaml_config::Schema ManagerState::GetStaticConfigSchema() {
  return userver::yaml_config::MergeSchemas<
      ComponentBase>(R"(
type: object
description: Manager config component
additionalProperties: false
properties:
    workers:
        type: array
        description: array or workers
        items:
            type: object
            description: worker info
            additionalProperties: false
            properties:
                host:
                    type: string
                    description: worker ip address
                port:
                    type: number
                    description: worker port
)");
}

ManagerState::ManagerState(const userver::components::ComponentConfig& config,
                           const userver::components::ComponentContext& context)
  :ComponentBase(config, context),
  queue( userver::concurrent::MpscQueue<ManagerTask>::Create()),
  consumer(queue->GetConsumer()){
  const auto workers =  config["workers"];
  workers.CheckArray();
  for (const auto& worker: workers) {
    worker.CheckObject();
    this->workers.emplace_back(worker["host"].As<std::string>(), worker["port"].As<std::uint16_t>(),
      context.FindComponent<userver::components::HttpClient>().GetHttpClient());
  }
  task = std::make_unique<userver::engine::Task>(
    userver::utils::AsyncBackground(
      "manger demon",
      context.GetTaskProcessor("man-task-processor"),
      [this] () {
          this->demonMain();
      }
    ));
}

bool ManagerState::addTask(const ManagerTask& task) {
  const auto p = queue->GetProducer();
  auto s = storage.UniqueLock();
  auto res = s->emplace(task.getId(), ManagerTaskResult());
  if (!res.second) {
    LOG_INFO() << "task wasn't added " << task.getId();
    return false;
  }
  if (!p.Push(ManagerTask(task))) {
    s->erase(task.getId());
    LOG_INFO() << "task wasn't added " << task.getId();
    return false;
  }
  LOG_INFO() << "task is successfully added " << task.getId();
  return true;
}

std::optional<ManagerTaskResult> ManagerState::getTaskResult(
    const ManagerTask::TaskId& taskId) const {
  auto s = storage.SharedLock();
  if (auto res = s->find(taskId); res != s->end()) {
    return {res->second};
  }
  return {};
}

void ManagerState::demonMain() {
  while (!userver::engine::current_task::ShouldCancel()) {
    addTasksToWorkers();
    userver::engine::InterruptibleSleepFor(std::chrono::seconds(1));
    updateWorkerStatuses();
  }

}

void ManagerState::updateShortQueue() {
  while (shortQueue.size() < workers.size()) {
    if (!currentTask.has_value() || currentTask->isDone()) {
      ManagerTask task;
      auto popRes = consumer.PopNoblock(task);
      if (!popRes) {
        return;
      }
      currentTask.emplace(task);
    }
    shortQueue.push_back({currentTask->getNextPart(), currentTask->getId()});
    LOG_INFO() << "pushing task to short queue task id: " << shortQueue.back().taskId << " part : " << shortQueue.back().part;
  }
}

void ManagerState::addTasksToWorkers() {
  updateShortQueue();
  for (auto& worker : workers) {
    if (shortQueue.empty())
      break;

    if (worker.getLastWorkerStatus() != WorkerState::IN_PROGRESS) {
      if (worker.addTask({shortQueue.front().taskId, shortQueue.front().part}))
        shortQueue.pop_front();
    }
  }
}

void ManagerState::updateWorkerStatuses() {
  for (auto& worker: workers){
    if (worker.getLastWorkerStatus() != WorkerState::IN_PROGRESS)
      continue;

    worker.updateStatus();
    if (worker.getLastWorkerStatus() == WorkerState::DONE) {
      if (worker.getLastTaskResult() != nullptr) {
        cancelTasks(worker.getWorkerTask()->id);
        saveTaskResult(worker.getWorkerTask()->id, *(worker.getLastTaskResult()));
      }
      else {
        checkOnTaskFullComplete(worker.getWorkerTask()->id);
      }
    }

    if (worker.getLastWorkerStatus() == WorkerState::DEAD && worker.getWorkerTask() != nullptr) {
      shortQueue.push_back({worker.getWorkerTask()->taskPart, worker.getWorkerTask()->id});
    }
  }

}

void ManagerState::cancelTasks(const ManagerTask::TaskId& taskId) {
  if (currentTask.has_value() && !currentTask->isDone() && currentTask->getId() == taskId) {
    currentTask.reset();
  }
  for (auto it = shortQueue.begin(); it != shortQueue.end(); ++it) {
    if (it->taskId == taskId) {
      it = shortQueue.erase(it);
    }
  }

  for (auto& worker : workers) {
    if (worker.getLastWorkerStatus() == WorkerState::IN_PROGRESS &&
        worker.getWorkerTask()->id == taskId) {
      worker.cancelTask();
    }
  }
}

void ManagerState::saveTaskResult(const ManagerTask::TaskId& taskId,
                                  const std::string& result) {
  auto s = storage.UniqueLock();
  const auto entry = s->find(taskId);
  if (entry == s->end()) {
    LOG_WARNING() << "found result for unexpected task id: " << taskId;
    return;
  }
  entry->second.type = FOUND;
  entry->second.result = result;
  LOG_INFO() << "task " << taskId << " result was found" << result;
}

void ManagerState::checkOnTaskFullComplete(const ManagerTask::TaskId& taskId) {
  if (currentTask.has_value() && currentTask->getId() == taskId && !currentTask->isDone())
    return;

  for (const auto& workerTask: shortQueue) {
    if (workerTask.taskId == taskId)
      return;
  }

  auto s = storage.UniqueLock();
  const auto entry = s->find(taskId);
  if (entry == s->end()) {
    LOG_WARNING() << " did not find result for unexpected task id: " << taskId;
    return;
  }
  entry->second.type = NOT_FOUND;
  LOG_INFO() << "task " << taskId << " result was not found";
}



} //manager