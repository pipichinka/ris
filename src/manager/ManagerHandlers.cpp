//
// Created by user on 3/11/25.
//

#include "ManagerHandlers.h"

#include <userver/server/handlers/exceptions.hpp>
#include <userver/server/handlers/http_handler_base.hpp>
#include <userver/server/handlers/http_handler_json_base.hpp>

#include "ManagerState.h"
#include "userver/components/component_context.hpp"
#include "userver/utils/log.hpp"

#include "src/dto/manager.hpp"
#include "utils/HandlerUtils.h"

using namespace manager;
using namespace  userver::server;

class TaskSolveHandler final : public handlers::HttpHandlerJsonBase {
private:
  ManagerState& managerState;
public:
  static constexpr std::string_view kName = "handler-manger-solve";
  using base = HttpHandlerJsonBase;

  TaskSolveHandler(const userver::components::ComponentConfig& config, const userver::components::ComponentContext& context):
  base(config, context),
  managerState(context.FindComponent<ManagerState>())
  {
  }

  userver::formats::json::Value HandleRequestJsonThrow(
      const http::HttpRequest& request,
      const userver::formats::json::Value& request_json,
      request::RequestContext& context) const override {
    const auto solveTaskDto = parseJson<dto::ManagerSolveRequest>(request_json);
    const auto task = ManagerTask(solveTaskDto.hash, solveTaskDto.length);
    if (!task.isValid()) {
      throw handlers::ClientError(
        DetailedErrorBuilder{"invalid Md5 task"});
    }
    if (managerState.addTask(task)) {
      const dto::ManagerSolveResponse response {task.getId()};
      return userver::formats::json::ValueBuilder(response).ExtractValue();
    }

    throw handlers::ConflictError(
      DetailedErrorBuilder{"can't get more tasks"});
  }
};

class TaskStatusHandler final : public handlers::HttpHandlerJsonBase {
private:
  ManagerState& managerState;
public:
  static constexpr std::string_view kName = "handler-manger-status";
  using base = HttpHandlerJsonBase;

  TaskStatusHandler(const userver::components::ComponentConfig& config, const userver::components::ComponentContext& context):
  base(config, context),
  managerState(context.FindComponent<ManagerState>())
  {
  }

  userver::formats::json::Value HandleRequestJsonThrow(
      const http::HttpRequest& request,
      const userver::formats::json::Value& request_json,
      request::RequestContext& context) const override {
    const auto statusTaskDto = parseJson<dto::ManagerStatusRequest>(request_json);
    const auto res = managerState.getTaskResult(statusTaskDto.task_id);

    if (!res.has_value()) {
      throw handlers::ResourceNotFound(
        DetailedErrorBuilder{"no such task"});
    }

    dto::ManagerStatusResponse response{
      response.status = std::string( ManagerTaskResultTypeToString(res.value().type))
    };

    if (res.value().type == FOUND) {
      response.result = res.value().result;
    }

    return userver::formats::json::ValueBuilder(response).ExtractValue();
  }
};

void manager::AppendMangerHandlers(userver::components::ComponentList &component_list) {
  component_list.Append<TaskSolveHandler>();
  component_list.Append<TaskStatusHandler>();
}