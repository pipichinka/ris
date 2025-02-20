#ifndef BACKGROUNDTASKPROCESSOR_H
#define BACKGROUNDTASKPROCESSOR_H

#include <unordered_map>

#include "task/Md5PartSolver.h"
#include "userver/concurrent/variable.hpp"
#include "userver/engine/task/task_with_result.hpp"
#include "userver/utils/boost_uuid4.hpp"
namespace worker {

enum TaskResultType {IN_PROGRESS, FOUND, NOT_FOUND, CANCELED, NO_SUCH_TASK};

struct TaskResult {
  std::string result;
  TaskResultType type;
  TaskResult(): type(IN_PROGRESS){}
};

class BackgroundTaskProcessor {
public:
  using TaskId = boost::uuids::uuid;
  using Storage = std::unordered_map<TaskId, TaskResult>;

  explicit BackgroundTaskProcessor(userver::engine::TaskProcessor& taskProcessor);

  bool addTask(const task::Md5Part& t);
  bool cancelTaskById(const TaskId& id) const;
  TaskResult getTaskResult(const TaskId& id) const;

private:
  void ProcessTask(const task::Md5Part& t,const TaskId& id);
  userver::concurrent::Variable<Storage> storage;
  userver::engine::Task* task;
  boost::uuids::uuid currentTaskUuid;
  userver::engine::TaskProcessor& taskProcessor;
};

} // worker

#endif //BACKGROUNDTASKPROCESSOR_H
