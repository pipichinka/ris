//
// Created by user on 3/4/25.
//

#ifndef MANAGERSTATE_H
#define MANAGERSTATE_H

#include <queue>
#include <unordered_map>
#include <userver/components/component_base.hpp>
#include <userver/concurrent/mpsc_queue.hpp>
#include <userver/concurrent/variable.hpp>
#include <userver/engine/task/task.hpp>
#include "ManagerTask.h"
#include "WorkerState.h"
#include "userver/engine/shared_mutex.hpp"
namespace manager {

class ManagerState final : public userver::components::ComponentBase {
public:
  static constexpr std::string_view kName = "manager-state";
private:

  struct StorageHash {
    size_t operator()(const ManagerTask::TaskId& t) const noexcept {
      return hash_value(t);
    }
  };

  struct ManagerTaskPart {
    task::Md5Part part;
    ManagerTask::TaskId taskId{};
  };

  using Queue = std::shared_ptr<userver::concurrent::MpscQueue<ManagerTask>>;
  using ShortQueue = std::deque<ManagerTaskPart>;
  using Storage = std::unordered_map<ManagerTask::TaskId, ManagerTaskResult, StorageHash>;
  using ProtectedStorage = userver::concurrent::Variable<Storage, userver::engine::SharedMutex>;

public:


  ManagerState(const userver::components::ComponentConfig& config, const userver::components::ComponentContext& context);
  ~ManagerState() override = default;

  bool addTask(const ManagerTask& task);
  std::optional<ManagerTaskResult> getTaskResult(const ManagerTask::TaskId& taskId) const;
  static userver::yaml_config::Schema GetStaticConfigSchema();



private:
  Queue queue;
  ShortQueue shortQueue;
  ProtectedStorage storage;
  std::optional<ManagerTask> currentTask;
  std::vector<WorkerState> workers;
  std::unique_ptr<userver::engine::Task> task;
  Queue::element_type::Consumer consumer;

  [[noreturn]] void demonMain();

  void updateShortQueue();

  void addTasksToWorkers();

  void updateWorkerStatuses();

  void cancelTasks(const ManagerTask::TaskId& taskId);

  void checkOnTaskFullComplete(const ManagerTask::TaskId& taskId);

  void saveTaskResult(const ManagerTask::TaskId& taskId, const std::string& result);
};

}//namespace manager






#endif //MANAGERSTATE_H
