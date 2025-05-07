//
// Created by user on 5/5/25.
//

#include "TaskConsumer.h"

#include <task/TaskResult.h>
#include <utils/HandlerUtils.h>

#include <src/dto/worker.hpp>
#include <utility>

#include "Storage.h"
#include "userver/components/component_context.hpp"
#include "userver/engine/sleep.hpp"
#include "userver/formats/json/value_builder.hpp"
#include "userver/formats/serialize/boost_uuid.hpp"
#include "userver/urabbitmq/client.hpp"
#include "userver/urabbitmq/consumer_base.hpp"
#include "userver/yaml_config/merge_schemas.hpp"
#include "userver/urabbitmq/component.hpp"
namespace manager {

class MessagePublisher {
private:
  userver::urabbitmq::Exchange exchange;
  std::string routingKey;
  std::shared_ptr<userver::urabbitmq::Client> client;
public:
  MessagePublisher(userver::urabbitmq::Exchange exchange,
                   std::string routing_key,
                   const std::shared_ptr<userver::urabbitmq::Client>& channel)
      : exchange(std::move(exchange)), routingKey(std::move(routing_key)), client(channel) {}

  void publish(const std::string& message) const {
    client->PublishReliable(exchange, routingKey, message, userver::urabbitmq::MessageType::kPersistent, userver::engine::Deadline());
  }
};

class TaskPartConsumer final: public TaskPartConsumerBase {
public:
  explicit TaskPartConsumer(MessagePublisher publisher):
    publisher(std::move(publisher)){}
  ~TaskPartConsumer() override = default;

  void onTaskPart(ManagerTaskPart part) override {
    const dto::WorkerLaunchTaskRequest dto {
      .task_id = part.taskPartId,
      .hash = part.part.hash,
      .start = part.part.start,
      .count = part.part.count,};


    while (true) {
      try {
        LOG_INFO() << "send message to task queue" << ToString(userver::formats::json::ValueBuilder(dto).ExtractValue());
        publisher.publish(ToString(userver::formats::json::ValueBuilder(dto).ExtractValue()));
        break;
      } catch (const std::exception& e) {
        LOG_WARNING() << "can't send task to worker: " <<e.what() << ". Retry in 10 sec";
        userver::engine::InterruptibleSleepFor(std::chrono::seconds(5));
      }
    }
  }
private:
  MessagePublisher publisher;
};


class TaskResultConsumer final: public userver::urabbitmq::ConsumerBase {
public:
  TaskResultConsumer(const std::shared_ptr<userver::urabbitmq::Client>& client,
                     const userver::urabbitmq::ConsumerSettings& settings, Storage& storage)
      : ConsumerBase(client, settings), storage(storage) {}

  ~TaskResultConsumer() override = default;

protected:
  void Process(const std::string mes) override {
    LOG_INFO() << "get result " << mes;
    const auto taskResultDto = parseJson<dto::WorkerTaskResult>(userver::formats::json::FromString(mes));

    if (task::TaskResultTypeFromString(taskResultDto.status) == task::TaskResultType::CANCELED)
      return;
    storage.putTaskPartResult(taskResultDto.task_id, taskResultDto.result);
  }

private:
  Storage& storage;
};


class TaskCancelConsumer final: public TaskCancelConsumerBase {
 public:
  explicit TaskCancelConsumer(MessagePublisher  publisher): publisher(std::move(publisher)){}
  ~TaskCancelConsumer() override = default;

  void onTaskCancel(const boost::uuids::uuid taskid) override {
    const dto::WorkerKillTaskRequest dto{.task_id = taskid};
    try {
      LOG_INFO() << "send message to cancel queue" << ToString(userver::formats::json::ValueBuilder(dto).ExtractValue());
      publisher.publish(ToString(userver::formats::json::ValueBuilder(dto).ExtractValue()));
    }
    catch (const std::exception& e) {
      LOG_INFO() << "can't send cancel to worker: " << e.what();
    }
  }

private:
  MessagePublisher publisher;
};


class ConsumerHolder final: public userver::components::ComponentBase {
public:
  static constexpr std::string_view kName = "consumer-service";
  ConsumerHolder(const userver::components::ComponentConfig& config,
                const userver::components::ComponentContext& context)
     : ComponentBase(config, context) {
    auto& storage = context.FindComponent<Storage>();
    rabbitClient = context.FindComponent<userver::components::RabbitMQ>("rabbitmq").GetClient();

    //getting settings
    const auto exchangeAdd = userver::urabbitmq::Exchange(config["queue_add_exchange"].As<std::string>());
    const auto exchangeCancel = userver::urabbitmq::Exchange(config["queue_cancel_exchange"].As<std::string>());
    const auto queueResult = config["queue_result"].As<std::string>();
    const auto queueAdd = config["queue_add"].As<std::string>();
    const auto resultConsumerSettings = userver::urabbitmq::ConsumerSettings(userver::urabbitmq::Queue(queueResult), 1);


    //making queues
    rabbitClient->DeclareQueue(userver::urabbitmq::Queue(queueAdd),
        userver::utils::Flags{userver::urabbitmq::Queue::Flags::kDurable}, userver::engine::Deadline());

    rabbitClient->DeclareQueue(userver::urabbitmq::Queue(queueResult),
        userver::utils::Flags{userver::urabbitmq::Queue::Flags::kDurable}, userver::engine::Deadline());

    //making exchange

    rabbitClient->DeclareExchange(exchangeAdd, userver::urabbitmq::Exchange::Type::kDirect,
      {userver::urabbitmq::Exchange::Flags::kAutoDelete}, userver::engine::Deadline());
    rabbitClient->BindQueue(exchangeAdd, userver::urabbitmq::Queue(queueAdd), queueAdd, userver::engine::Deadline());

    rabbitClient->DeclareExchange(exchangeCancel, userver::urabbitmq::Exchange::Type::kFanOut,
          {userver::urabbitmq::Exchange::Flags::kAutoDelete}, userver::engine::Deadline());

    taskCancelConsumer = std::make_shared<TaskCancelConsumer>(MessagePublisher(exchangeCancel, "", rabbitClient));
    taskResultConsumer = std::make_unique<TaskResultConsumer>(rabbitClient, resultConsumerSettings, storage);
    taskPartConsumer = std::make_shared<TaskPartConsumer>(MessagePublisher(exchangeAdd, queueAdd, rabbitClient));

    taskResultConsumer->Start();
    storage.addTaskCancelConsumer(taskCancelConsumer);
    storage.addTaskPartConsumer(taskPartConsumer);
  }

 ~ConsumerHolder() override {
    taskResultConsumer->Stop();
  }
  static userver::yaml_config::Schema GetStaticConfigSchema() {
    return userver::yaml_config::MergeSchemas<
      ComponentBase>(R"(
type: object
description: Consumer config schema
additionalProperties: false
properties:
    queue_add:
        type: string
        description: name of queue with available tasks
    queue_add_exchange:
        type: string
        description: name of exchange that will be bound to add queue
    queue_cancel_exchange:
        type: string
        description: name of exchange that will be bound to cancel queue
    queue_result:
        type: string
        description: name of queue with task results
)");
  }
private:
  std::shared_ptr<userver::urabbitmq::Client> rabbitClient;
  std::shared_ptr<TaskCancelConsumer> taskCancelConsumer;
  std::unique_ptr<TaskResultConsumer> taskResultConsumer;
  std::shared_ptr<TaskPartConsumer> taskPartConsumer;

};


void AppendConsumers(userver::components::ComponentList &component_list) {
  component_list.Append<ConsumerHolder>();
}
}