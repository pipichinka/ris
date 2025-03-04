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
  if (task != nullptr && !task->IsFinished()) {
    LOG_INFO() << "worker is busy";
    return nullptr;
  }
  auto s = storage.UniqueLock();

  if (task != nullptr) {
    if (!task->IsFinished()) {
      LOG_INFO() << "worker is busy";
      return nullptr;
    }
    task = nullptr;
  }
  LOG_INFO() << "adding task " << t;
  currentTaskId = userver::utils::generators::GenerateBoostUuid();

  s->emplace(currentTaskId, TaskResult());

  task = std::make_unique<userver::engine::Task>( userver::utils::Async(taskProcessor, "md5 task",
    [this] (const task::Md5Part& task, const TaskId& id)  {
      this->ProcessTask(task, id);
    },
    t, currentTaskId));

  return new TaskId(currentTaskId);
}

TaskResult BackgroundTaskProcessor::getTaskResult(const TaskId& id) const {
  LOG_INFO() << "looking for task " << id;
  auto s = storage.SharedLock();
  const auto res = s->find(id);
  if (res == s->end()) {
    LOG_INFO() << "did not find task " << id;
    TaskResult r;
    r.type = NO_SUCH_TASK;
    return r;
  }
  LOG_INFO() << "found task "  << res->second;
  return res->second;
}

bool BackgroundTaskProcessor::cancelTaskById(const TaskId& id) {
  auto lock =  storage.UniqueLock();
  if (currentTaskId != id || task == nullptr) {
    return false;
  }
  const auto copy = std::move(task);
  lock.GetLock().unlock();
  LOG_INFO() << "request cancel on task " << id;
  copy->RequestCancel();
  LOG_INFO() << "wait for task to finish";
  auto waitRes = copy->WaitNothrow();
  LOG_INFO() << "task is finished is canceled = " << waitRes;
  return true;
}


void BackgroundTaskProcessor::ProcessTask(const task::Md5Part& t,
                                          const TaskId& id) {
  auto solver = task::Md5PartSolver(t);
  LOG_INFO() << "starting task " << t << "with task id " << id;
  const bool r = solver.solve();
  auto s = storage.UniqueLock();
  auto entry = s->find(id);
  currentTaskId = TaskId();
  if (r) {
    LOG_INFO() << "found result " << solver.result();
    entry->second.result = solver.result();
    entry->second.type = FOUND;
    return;
  }

  if (solver.isCancelled()) {
    LOG_INFO() << "task was canceled";
    entry->second.type = CANCELED;
    return;
  }
  LOG_INFO() << "result is not found";
  entry->second.type = NOT_FOUND;
}

} // worker