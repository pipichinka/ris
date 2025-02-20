#include "BackgroundTaskProcessor.h"

#include <boost/uuid/random_generator.hpp>

#include "userver/utils/async.hpp"

namespace worker {

BackgroundTaskProcessor::BackgroundTaskProcessor(userver::engine::TaskProcessor& taskProcessor):
  task(nullptr),
  currentTaskUuid(),
  taskProcessor(taskProcessor){

}

bool BackgroundTaskProcessor::addTask(const task::Md5Part& t) {
  if (task != nullptr) {
    return false;
  }
  auto s = storage.UniqueLock();
  if (task != nullptr) {
    return false;
  }

  auto id = userver::utils::generators::GenerateBoostUuid();

  s->emplace(id, TaskResult());

  task = new userver::engine::Task(userver::utils::Async(taskProcessor, "md5 task",
    [this] (const task::Md5Part& task, const TaskId& id)  {
      this->ProcessTask(task, id);
    },
    t, id));
  currentTaskUuid = id;
  return true;
}

TaskResult BackgroundTaskProcessor::getTaskResult(const TaskId& id) const {
  auto s = storage.SharedLock();
  const auto res = s->find(id);
  if (res != s->end()) {
    TaskResult r;
    r.type = NO_SUCH_TASK;
    return r;
  }

  return res->second;
}

bool BackgroundTaskProcessor::cancelTaskById(const TaskId& id) const {
  if (currentTaskUuid != id) {
    return false;
  }
  task->RequestCancel();
  return true;
}


void BackgroundTaskProcessor::ProcessTask(const task::Md5Part& t,
                                          const TaskId& id) {
  auto solver = task::Md5PartSolver(t);
  const bool r = solver.solve();
  auto s = storage.UniqueLock();
  auto entry = s->find(id);
  if (r) {
    entry->second.result = solver.result();
    entry->second.type = FOUND;
    return;
  }

  if (solver.isCancelled()) {
    entry->second.type = CANCELED;
    return;
  }
  entry->second.type = NOT_FOUND;
}

} // worker