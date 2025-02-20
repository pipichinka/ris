#ifndef BACKGROUNDTASKPROCESSOR_H
#define BACKGROUNDTASKPROCESSOR_H

#include <unordered_map>

#include "task/Md5PartSolver.h"
#include "userver/concurrent/variable.hpp"
#include "userver/utils/boost_uuid4.hpp"
#include "userver/engine/shared_mutex.hpp"
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

  struct StorageHash {
    size_t operator()(const TaskId& t) const noexcept {
      return boost::uuids::hash_value(t);
    }
  };

  using Storage = std::unordered_map<TaskId, TaskResult, StorageHash>;

  explicit BackgroundTaskProcessor(userver::engine::TaskProcessor& taskProcessor);

  bool addTask(const task::Md5Part& t);
  bool cancelTaskById(const TaskId& id) const;
  TaskResult getTaskResult(const TaskId& id) const;

private:
  void ProcessTask(const task::Md5Part& t,const TaskId& id);
  userver::concurrent::Variable<Storage, userver::engine::SharedMutex> storage;
  userver::engine::Task* task;
  boost::uuids::uuid currentTaskUuid;
  userver::engine::TaskProcessor& taskProcessor;
};

} // worker

#endif //BACKGROUNDTASKPROCESSOR_H
