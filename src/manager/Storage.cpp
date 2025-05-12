//
// Created by user on 4/23/25.
//

#include "Storage.h"

#include "userver/components/component_context.hpp"
#include "userver/utils/async.hpp"
#include "userver/engine/sleep.hpp"
#include <userver/formats/bson/inline.hpp>

#include "userver/formats/serialize/boost_uuid.hpp"
#include "boost/lexical_cast.hpp"

#define MAX_TASKS_IN_QUEUE 10

namespace manager {

using userver::formats::bson::Document;
using userver::formats::bson::MakeDoc;
 Storage::Storage(
    const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context): ComponentBase(config, context),
      tasksInQueue(0),
      taskProcessor(context.GetTaskProcessor("man-task-processor")),
      mongo(context.FindComponent<userver::components::Mongo>("mongo").GetPool()){

}

void Storage::addTask(const ManagerTask& task) const {
   auto collectionTask = mongo->GetCollection("tasks");
   auto collectionProgress = mongo->GetCollection("task_progress");
   const auto id = userver::formats::serialize::detail::ToString(task.getId());
   try {
     auto res = collectionTask.InsertOne(
       MakeDoc(
         "id", id,
         "status", ManagerTaskResultTypeToString(IN_PROGRESS),
         "result", "")
         );

     collectionProgress.InsertOne(MakeDoc(
       "id", id,
       "hash", task.getPartMaker().getHash(),
       "start", task.getPartMaker().getCurrentStartString()
       ));
   } catch (const std::exception& ex) {
     collectionTask.DeleteOne(MakeDoc("id", id));
     LOG_ERROR() << "can't create new task: " <<ex.what();
     throw;
   }
 }


std::optional<ManagerTaskResult> Storage::getTaskResult(const ManagerTask::TaskId& id) {
   auto collectionTask = mongo->GetCollection("tasks");
   auto res = collectionTask.FindOne(MakeDoc("id", userver::formats::serialize::detail::ToString(id)));
   if (!res.has_value())
     return {};

   ManagerTaskResult result;
   result.type = ManagerTaskResultTypeFromString(res.value()["status"].As<std::string>());

   if (result.type == FOUND)
     result.result = res.value()["result"].As<std::string>();

   return {result};
 }


void Storage::updateCurrentTasksInQueue() {
   const auto collection = mongo->GetCollection("queue");
   tasksInQueue = collection.Count({});
 }


void Storage::storageDemonWrapper() {
   while (!userver::engine::current_task::IsCancelRequested()) {
     try {
       storageDemon();
     }
     catch (const std::exception& ex) {
       LOG_ERROR() << "got error at background demon: " << ex.what() << ". Wait 5 sec then restart.";
       currentTask.reset();
       tasksInQueue = 0;
       userver::engine::InterruptibleSleepFor(std::chrono::seconds(5));
     }
   }
 }

void Storage::addTaskPartConsumer(const std::shared_ptr<TaskPartConsumerBase>& consumer) {
   consumerTaskPart = consumer;
   task = std::make_unique<userver::engine::Task>(
   userver::utils::AsyncBackground(
     "manger demon",
     taskProcessor,
     [this] () {
         this->storageDemonWrapper();
     }
   ));
 }


ManagerTaskPart Storage::getNextPart() {
   while (!userver::engine::current_task::IsCancelRequested()) {
     if (tasksInQueue >= MAX_TASKS_IN_QUEUE) {
       userver::engine::InterruptibleSleepFor(std::chrono::seconds(1));
       continue;
     }
     if (!currentTask.has_value() || currentTask->isDone()) {
       auto collection = mongo->GetCollection("task_progress");
       const auto res = collection.FindOne({});
       if (!res) {
         userver::engine::InterruptibleSleepFor(std::chrono::seconds(1));
         continue;
       }


       currentTask = ManagerTask(
         userver::utils::BoostUuidFromString( res.value()["id"].As<std::string>()),
         res.value()["hash"].As<std::string>(),
         res.value()["start"].As<std::string>());
     }
     else if (mongo->GetCollection("task_progress").Count(
              MakeDoc("id", userver::formats::serialize::detail::ToString(currentTask->getId()))) == 0) { //check on task end
       currentTask.reset();
       continue;
     }

     ManagerTaskPart result = {currentTask->getNextPart(), userver::utils::generators::GenerateBoostUuid()};

     auto collectionQueue = mongo->GetCollection("queue");
     const auto id = userver::formats::serialize::detail::ToString(currentTask->getId());
     collectionQueue.InsertOne(MakeDoc(
        "id", id,
        "partId", userver::formats::serialize::detail::ToString(result.taskPartId)
     ));
     tasksInQueue++;
     return result;
   }
   throw std::runtime_error("task is canceled");
 }


void Storage::saveCurrentState() {
   auto collectionTasksProgress = mongo->GetCollection("task_progress");
   const auto id = userver::formats::serialize::detail::ToString(currentTask->getId());
   if (!currentTask->isDone()) {
     collectionTasksProgress.UpdateOne(
     MakeDoc("id", id),
     MakeDoc("$set", MakeDoc("start", currentTask->getPartMaker().getCurrentStartString())));
   }
   else {
     collectionTasksProgress.DeleteOne(MakeDoc("id", id));
   }
 }


void Storage::putTaskPartResult(const boost::uuids::uuid& taskPartId,
                                 const std::optional<std::string>& result) {
   const auto partId = userver::formats::serialize::detail::ToString(taskPartId);
   LOG_INFO() << "Task part result " << taskPartId << " has value: " << result.has_value();
   auto collectionQueue = mongo->GetCollection("queue");
   const auto taskPartDoc = collectionQueue.FindOne(MakeDoc("partId", partId));
   if (!taskPartDoc.has_value()) {
     LOG_WARNING() << "got unexpected task result " << partId;
     return;
   }
   auto id = taskPartDoc.value()["id"].As<std::string>();
   const bool isTaskFinished = (mongo->GetCollection("task_progress").Count(MakeDoc("id", id)) == 0 &&
                        collectionQueue.Count(MakeDoc("id", id)) == 1) || result.has_value();

   if (!isTaskFinished) {
     collectionQueue.DeleteOne(MakeDoc("partId", partId));
     tasksInQueue--;
     return;
   }

   auto collectionTasks = mongo->GetCollection("tasks");
   if (result.has_value()) {
     collectionTasks.UpdateOne(MakeDoc("id", id),
       MakeDoc("$set", MakeDoc("result", result.value(), "status", ManagerTaskResultTypeToString(FOUND))));
     LOG_INFO() << "Result is found for task " << id << " : " << result.value();
     mongo->GetCollection("task_progress").DeleteOne(MakeDoc("id", id));
     cancelTasks(id);
     return;
   }

   collectionTasks.UpdateOne(MakeDoc("id", id),
     MakeDoc("$set", MakeDoc("status", ManagerTaskResultTypeToString(NOT_FOUND))));
   LOG_INFO() << "Result is not found for task " << id;
   collectionQueue.DeleteOne(MakeDoc("partId", partId));
   tasksInQueue--;
 }


void Storage::cancelTasks(const std::string& id) {
   auto collectionQueue = mongo->GetCollection("queue");
   auto res = collectionQueue.Find(MakeDoc("id", id));
   for (const auto& item: res) {
     if (consumerTaskCancel != nullptr)
       consumerTaskCancel->onTaskCancel(userver::utils::BoostUuidFromString(item["partId"].As<std::string>()));
   }
   tasksInQueue -= collectionQueue.DeleteMany(MakeDoc("id", id)).DeletedCount();
 }



void Storage::storageDemon() {
   updateCurrentTasksInQueue();
   while (!userver::engine::current_task::IsCancelRequested()) {
      const auto part = getNextPart();
     LOG_INFO() << "part is ready to send" << part.taskPartId;
      if (consumerTaskPart != nullptr)
        consumerTaskPart->onTaskPart(part);
      saveCurrentState();
   }
 }


} // manager