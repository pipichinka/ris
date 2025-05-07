//
// Created by user on 4/23/25.
//

#ifndef STORAGE_H
#define STORAGE_H
#include "userver/storages/mongo/component.hpp"
#include "ManagerTask.h"

namespace manager {

struct ManagerTaskPart {
  task::Md5Part part;
  boost::uuids::uuid taskPartId;

  ManagerTaskPart(const task::Md5Part& part,
                  const boost::uuids::uuid& taskPartId)
      : part(part), taskPartId(taskPartId) {}
};


class TaskPartConsumerBase {
public:
  virtual void onTaskPart(ManagerTaskPart part) = 0;

  virtual ~TaskPartConsumerBase() = default;
};


class TaskCancelConsumerBase {
public:
  virtual void onTaskCancel(boost::uuids::uuid taskid) = 0;

  virtual ~TaskCancelConsumerBase() = default;
};

class Storage: public userver::components::ComponentBase{
public:
  static constexpr std::string_view kName = "storage-service";
  Storage(const userver::components::ComponentConfig& component_config,
          const userver::components::ComponentContext& component_context);

  void addTask(const ManagerTask& task) const;
  std::optional<ManagerTaskResult> getTaskResult(const ManagerTask::TaskId& id);
  void putTaskPartResult(const boost::uuids::uuid& taskPartId, const std::optional<std::string>& result);

  void addTaskPartConsumer(
      const std::shared_ptr<TaskPartConsumerBase>& consumer);
  void addTaskCancelConsumer(
      const std::shared_ptr<TaskCancelConsumerBase>& consumer) {consumerTaskCancel = consumer;}

  ~Storage() override = default;
private:

  void storageDemonWrapper();

  void storageDemon();

  void updateCurrentTasksInQueue();

  ManagerTaskPart getNextPart();

  void saveCurrentState();

  void cancelTasks(const std::string& id);
  size_t tasksInQueue;
  std::optional<ManagerTask> currentTask;
  userver::engine::TaskProcessor& taskProcessor;
  userver::storages::mongo::PoolPtr mongo;
  std::shared_ptr<TaskPartConsumerBase> consumerTaskPart;
  std::shared_ptr<TaskCancelConsumerBase> consumerTaskCancel;
  std::unique_ptr<userver::engine::Task> task;
};

} // manager

#endif //STORAGE_H
