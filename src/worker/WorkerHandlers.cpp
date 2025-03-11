#include "WorkerHandlers.h"

#include <userver/server/handlers/http_handler_base.hpp>
#include <userver/server/handlers/http_handler_json_base.hpp>
#include <userver/server/handlers/exceptions.hpp>

#include "BackgroundTaskProcessor.h"
#include "src/dto/jeneral.hpp"
#include "src/dto/worker.hpp"
#include "userver/components/component_context.hpp"
#include "userver/utils/log.hpp"
#include "utils/HandlerUtils.h"

using namespace worker;
using namespace  userver::server;


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
    const auto launchTaskDto = parseJson<dto::WorkerLaunchTaskRequest>(request_json);
    const auto task = task::Md5Part(launchTaskDto.hash, launchTaskDto.start, launchTaskDto.count);
    if (!task.isValid()) {
      throw handlers::ClientError(
        DetailedErrorBuilder{"invalid Md5 task"});
    }

    BackgroundTaskProcessor::TaskId* addTaskRes = taskProcessor.addTask(task);

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
    const auto statusTaskDto = parseJson<dto::WorkerStatusTaskRequest>(request_json);
    const auto result = taskProcessor.getTaskResult(statusTaskDto.task_id);
    if (result.type == task::FOUND) {
      dto::WorkerStatusTaskResponse response {.result = result.result, .status = std::string(TaskResultTypeToString(result.type))};
      return userver::formats::json::ValueBuilder(response).ExtractValue();
    }
    dto::WorkerStatusTaskResponse response {.status = std::string(TaskResultTypeToString(result.type))};
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
    const auto killTaskDto = parseJson<dto::WorkerKillTaskRequest>(request_json);

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