#include "WorkerHandlers.h"

#include <manager/ManagerTask.h>

#include <userver/server/handlers/exceptions.hpp>
#include <userver/server/handlers/http_handler_base.hpp>
#include <userver/urabbitmq/consumer_base.hpp>
#include "userver/yaml_config/merge_schemas.hpp"
#include "userver/urabbitmq/component.hpp"
#include <utility>

#include "userver/urabbitmq/client.hpp"
#include "BackgroundTaskProcessor.h"
#include "src/dto/worker.hpp"
#include "utils/HandlerUtils.h"

using namespace worker;
using namespace  userver::server;


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


class TaskAddConsumer final: public userver::urabbitmq::ConsumerBase {
private:
  BackgroundTaskProcessor& taskProcessor;
  MessagePublisher& messagePublisher;
public:
  TaskAddConsumer(BackgroundTaskProcessor& taskProcessor,
                 MessagePublisher& messagePublisher,
                 const std::shared_ptr<userver::urabbitmq::Client>& client,
                 const userver::urabbitmq::ConsumerSettings& settings)
     : ConsumerBase(client, settings), taskProcessor(taskProcessor), messagePublisher(messagePublisher) {}

protected:
  void Process(const std::string mes) override {
    LOG_INFO() << "get message " << mes;
    const auto TaskLaunchDto = parseJson<dto::WorkerLaunchTaskRequest>(userver::formats::json::FromString(mes));
    taskProcessor.addTask(task::Md5Part(TaskLaunchDto.hash, TaskLaunchDto.start, TaskLaunchDto.count), TaskLaunchDto.task_id);
    LOG_INFO() << "task is solved" << mes;
    const auto res = taskProcessor.getTaskResult(TaskLaunchDto.task_id);

    dto::WorkerTaskResult resDto;
    resDto.status = TaskResultTypeToString(res.type);
    resDto.task_id = TaskLaunchDto.task_id;
    if (res.type == task::TaskResultType::FOUND) resDto.result = res.result;

    const std::string message = ToString(userver::formats::json::ValueBuilder(resDto).ExtractValue());
    messagePublisher.publish(message);
    LOG_INFO() << "message is processed" << mes;
  }
};


class TaskCancelConsumer final: public userver::urabbitmq::ConsumerBase {
private:
  BackgroundTaskProcessor& taskProcessor;
public:
  TaskCancelConsumer(BackgroundTaskProcessor& taskProcessor,
                 const std::shared_ptr<userver::urabbitmq::Client>& client,
                 const userver::urabbitmq::ConsumerSettings& settings)
     : ConsumerBase(client, settings), taskProcessor(taskProcessor) {}

protected:
  void Process(std::string mes) override {
    LOG_INFO() << "get message " << mes;
    const auto killTaskDto = parseJson<dto::WorkerKillTaskRequest>(userver::formats::json::FromString(mes));
    taskProcessor.cancelTaskById(killTaskDto.task_id);
  }
};


class ConsumerService final: public userver::components::ComponentBase {
private:
  std::unique_ptr<TaskAddConsumer> addConsumer;
  std::unique_ptr<MessagePublisher> messagePublisher;
  std::unique_ptr<TaskCancelConsumer> cancelConsumer;
  std::shared_ptr<userver::urabbitmq::Client> rabbitClient;
  BackgroundTaskProcessor& taskProcessor;
 public:
  static constexpr std::string_view kName = "consumer-service";
  ConsumerService(
      const userver::components::ComponentConfig& config,
      const userver::components::ComponentContext& context)
      : ComponentBase(config, context), taskProcessor(context.FindComponent<BackgroundTaskProcessor>()) {
    rabbitClient = context.FindComponent<userver::components::RabbitMQ>("rabbitmq").GetClient();

    //getting settings
    const auto exchange = userver::urabbitmq::Exchange(config["exchange_queue_result"].As<std::string>());
    const auto exchangeCancel = userver::urabbitmq::Exchange(config["queue_cancel_exchange"].As<std::string>());
    const auto queueResult = config["queue_result"].As<std::string>();
    const auto queueAdd = config["queue_add"].As<std::string>();
    const auto queueCancel = config["queue_cancel"].As<std::string>();
    const auto addConsumerSettings = userver::urabbitmq::ConsumerSettings(userver::urabbitmq::Queue(queueAdd), 1);
    const auto cancelConsumerSettings = userver::urabbitmq::ConsumerSettings(userver::urabbitmq::Queue(queueCancel), 1);

    //making queues
    rabbitClient->DeclareQueue(userver::urabbitmq::Queue(queueAdd),
        userver::utils::Flags{userver::urabbitmq::Queue::Flags::kDurable}, userver::engine::Deadline());

    rabbitClient->DeclareQueue(userver::urabbitmq::Queue(queueCancel),
        userver::utils::Flags{userver::urabbitmq::Queue::Flags::kDurable}, userver::engine::Deadline());

    rabbitClient->DeclareQueue(userver::urabbitmq::Queue(queueResult),
        userver::utils::Flags{userver::urabbitmq::Queue::Flags::kDurable}, userver::engine::Deadline());

    //making exchange

    rabbitClient->DeclareExchange(exchange, userver::urabbitmq::Exchange::Type::kDirect,
      {userver::urabbitmq::Exchange::Flags::kAutoDelete, userver::urabbitmq::Exchange::Flags::kDurable}, userver::engine::Deadline());
    rabbitClient->BindQueue(exchange, userver::urabbitmq::Queue(queueResult), queueResult, userver::engine::Deadline());

    rabbitClient->DeclareExchange(exchangeCancel, userver::urabbitmq::Exchange::Type::kFanOut,
          {userver::urabbitmq::Exchange::Flags::kAutoDelete, userver::urabbitmq::Exchange::Flags::kDurable}, userver::engine::Deadline());

    rabbitClient->BindQueue(exchangeCancel, userver::urabbitmq::Queue(queueCancel), queueCancel, userver::engine::Deadline());

    messagePublisher = std::make_unique<MessagePublisher>(exchange, queueResult, rabbitClient);
    addConsumer = std::make_unique<TaskAddConsumer>(taskProcessor, *messagePublisher, rabbitClient, addConsumerSettings);
    addConsumer->Start();
    cancelConsumer = std::make_unique<TaskCancelConsumer>(taskProcessor, rabbitClient, cancelConsumerSettings);
    cancelConsumer->Start();
  }

  ~ConsumerService() override {
    addConsumer->Stop();
    cancelConsumer->Stop();
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
    queue_cancel:
        type: string
        description: name of queue with cancel tasks
    queue_cancel_exchange:
        type: string
        description: name of exchange that will be bound to cancel queue
    queue_result:
        type: string
        description: name of queue with task results
    exchange_queue_result:
        type: string
        description: name of exchange to use for pushing into queue_result
)");
  }
};


void worker::AppendWorkerEndpoints(userver::components::ComponentList &component_list) {
  component_list.Append<ConsumerService>();
}


