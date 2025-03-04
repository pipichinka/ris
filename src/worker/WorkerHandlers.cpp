#include "WorkerHandlers.h"

#include <userver/server/handlers/http_handler_base.hpp>
#include <userver/server/handlers/http_handler_json_base.hpp>
#include <userver/server/handlers/exceptions.hpp>

#include "BackgroundTaskProcessor.h"
#include "src/dto/jeneral.hpp"
#include "src/dto/worker.hpp"
#include "userver/components/component_context.hpp"
#include "userver/utils/log.hpp"

using namespace worker;
using namespace  userver::server;

class DetailedErrorBuilder {
public:
  static constexpr bool kIsExternalBodyFormatted = true;

  explicit DetailedErrorBuilder(std::string msg) {
    dto::DetailedResponse response{.detail = std::move(msg)};
    body = ToString(userver::formats::json::ValueBuilder(response).ExtractValue());
  }

  [[nodiscard]] std::string GetExternalBody() const { return body; }

private:
  std::string body;
};

class TaskLaunchHandler final : public handlers::HttpHandlerJsonBase {
private:
  BackgroundTaskProcessor& taskProcessor;
public:
  static constexpr std::string_view kName = "handler-worker-launch";
  using base = HttpHandlerJsonBase;

  TaskLaunchHandler(const userver::components::ComponentConfig& config, const userver::components::ComponentContext& context):
  base(config, context),
  taskProcessor(context.FindComponent<BackgroundTaskProcessor>())
  {

  }

  userver::formats::json::Value HandleRequestJsonThrow(
      const http::HttpRequest& request,
      const userver::formats::json::Value& request_json,
      request::RequestContext& context) const override {
    const dto::WorkerLaunchTaskRequest launchTaskDto = request_json.As<dto::WorkerLaunchTaskRequest>();

    BackgroundTaskProcessor::TaskId* addTaskRes = taskProcessor.addTask(task::Md5Part(launchTaskDto.hash, launchTaskDto.start, launchTaskDto.count));

    if (addTaskRes) {
      dto::WorkerLaunchTaskResponse response;
      response.task_id = *addTaskRes;
      return userver::formats::json::ValueBuilder(response).ExtractValue();
    }

    throw handlers::ConflictError(
      DetailedErrorBuilder{"worker can't take more than one task"});
  }
};


class TaskStateHandler final : public handlers::HttpHandlerJsonBase {
private:
  BackgroundTaskProcessor& taskProcessor;
public:
  static constexpr std::string_view kName = "handler-worker-status";
  using base = HttpHandlerJsonBase;

  TaskStateHandler(const userver::components::ComponentConfig& config, const userver::components::ComponentContext& context):
  base(config, context),
  taskProcessor(context.FindComponent<BackgroundTaskProcessor>())
  {

  }

  userver::formats::json::Value HandleRequestJsonThrow(
      const http::HttpRequest& request,
      const userver::formats::json::Value& request_json,
      request::RequestContext& context) const override {
    const dto::WorkerStatusTaskRequest statusTaskDto = request_json.As<dto::WorkerStatusTaskRequest>();
    const auto result = taskProcessor.getTaskResult(statusTaskDto.task_id);
    if (result.type == FOUND) {
      dto::WorkerStatusTaskResponse response {.result = result.result, .status = TaskResultTypeToString(result.type)};
      return userver::formats::json::ValueBuilder(response).ExtractValue();
    }
    dto::WorkerStatusTaskResponse response {.status = TaskResultTypeToString(result.type)};
    return userver::formats::json::ValueBuilder(response).ExtractValue();
  }
};


class TaskKillHandler final : public handlers::HttpHandlerJsonBase {
private:
  BackgroundTaskProcessor& taskProcessor;
public:
  static constexpr std::string_view kName = "handler-worker-kill";
  using base = HttpHandlerJsonBase;

  TaskKillHandler(const userver::components::ComponentConfig& config, const userver::components::ComponentContext& context):
  base(config, context),
  taskProcessor(context.FindComponent<BackgroundTaskProcessor>())
  {

  }

  userver::formats::json::Value HandleRequestJsonThrow(
      const http::HttpRequest& request,
      const userver::formats::json::Value& request_json,
      request::RequestContext& context) const override {
    const dto::WorkerKillTaskRequest killTaskDto = request_json.As<dto::WorkerKillTaskRequest>();

    const auto result = taskProcessor.cancelTaskById(killTaskDto.task_id);

    dto::DetailedResponse response {.detail = (result ? "task is canceled" : "task is finished or no such rask") };
    return userver::formats::json::ValueBuilder(response).ExtractValue();
  }
};


void worker::AppendWorkerEndpoints(userver::components::ComponentList &component_list) {
  component_list.Append<TaskLaunchHandler>();
  component_list.Append<TaskStateHandler>();
  component_list.Append<TaskKillHandler>();
}