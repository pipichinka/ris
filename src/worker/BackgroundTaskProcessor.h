#ifndef BACKGROUNDTASKPROCESSOR_H
#define BACKGROUNDTASKPROCESSOR_H

#include <unordered_map>

#include "task/Md5PartSolver.h"
#include "userver/components/component_base.hpp"
#include "userver/components/component_context.hpp"
#include "userver/concurrent/variable.hpp"
#include "userver/engine/shared_mutex.hpp"
#include "userver/utils/boost_uuid4.hpp"

#include "task/TaskResult.h"
namespace worker {

class BackgroundTaskProcessor: public userver::components::ComponentBase{
public:
  using TaskId = boost::uuids::uuid;
  static constexpr std::string_view kName = "background-task-processor";

  struct StorageHash {
    size_t operator()(const TaskId& t) const noexcept {
      return hash_value(t);
    }
  };

  using Storage = std::unordered_map<TaskId, task::TaskResult, StorageHash>;

  explicit BackgroundTaskProcessor(const userver::components::ComponentConfig& config, const userver::components::ComponentContext& context);
  ~BackgroundTaskProcessor() override = default;
  boost::uuids::uuid* addTask(const task::Md5Part& t);
  [[nodiscard]] bool cancelTaskById(const TaskId& id);
  [[nodiscard]] task::TaskResult getTaskResult(const TaskId& id) const;

private:
  void ProcessTask(const task::Md5Part& t,const TaskId& id);
  userver::concurrent::Variable<Storage, userver::engine::SharedMutex> storage;
  std::unique_ptr<userver::engine::Task> task;
  userver::engine::TaskProcessor& taskProcessor;
  TaskId currentTaskId;
};

} // worker

#endif //BACKGROUNDTASKPROCESSOR_H
