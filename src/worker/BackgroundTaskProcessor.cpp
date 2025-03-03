#include "BackgroundTaskProcessor.h"

#include <boost/uuid/random_generator.hpp>

#include "fmt/chrono.h"
#include "userver/logging/log.hpp"
#include "userver/utils/async.hpp"

namespace worker {

std::string TaskResultTypeToString(TaskResultType type) {
  switch (type) {
    case IN_PROGRESS:
      return "IN_PROGRESS";
    case FOUND:
      return "FOUND";
    case NOT_FOUND:
      return "NOT_FOUND";
    case CANCELED:
      return "CANCELED";
    case NO_SUCH_TASK:
      return "NO_SUCH_TASK";
  }
  return "INVALID TYPE";
}

BackgroundTaskProcessor::BackgroundTaskProcessor(
    const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : ComponentBase(config, context),
      task(nullptr),
      taskProcessor(context.GetTaskProcessor("md5-task-processor")),
      currentTaskId() {}

boost::uuids::uuid* BackgroundTaskProcessor::addTask(const task::Md5Part& t) {
  if (task != nullptr) {
    return nullptr;
  }
  auto s = storage.UniqueLock();
  if (task != nullptr) {
    return nullptr;
  }

  currentTaskId = userver::utils::generators::GenerateBoostUuid();

  s->emplace(currentTaskId, TaskResult());

  task = new userver::engine::Task(userver::utils::Async(taskProcessor, "md5 task",
    [this] (const task::Md5Part& task, const TaskId& id)  {
      this->ProcessTask(task, id);
    },
    t, currentTaskId));
  return new TaskId(currentTaskId);
}

TaskResult BackgroundTaskProcessor::getTaskResult(const TaskId& id) const {
  auto s = storage.SharedLock();
  LOG_INFO() << "hello_inside 1";
  const auto res = s->find(id);
  LOG_INFO() << "hello_inside 1";
  if (res == s->end()) {
    LOG_INFO() << "hello_inside 1";
    TaskResult r;
    r.type = NO_SUCH_TASK;
    return r;
  }
  LOG_INFO() << "hello_inside 1";

  return res->second;
}

bool BackgroundTaskProcessor::cancelTaskById(const TaskId& id) const {
  if (currentTaskId != id) {
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
  currentTaskId = TaskId();
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