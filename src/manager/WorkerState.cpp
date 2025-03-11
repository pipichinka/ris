//
// Created by user on 3/4/25.
//

#include "WorkerState.h"

#include "ManagerTask.h"
#include "boost/format.hpp"
#include "src/dto/jeneral.hpp"
#include "src/dto/worker.hpp"
#include "task/TaskResult.h"
#include "userver/formats/json/value_builder.hpp"
namespace manager {

WorkerState::WorkerState(const std::string& host, std::uint16_t port, userver::clients::http::Client& client):
  workerTaskId(nullptr),
  httpClient(client),
  lastTaskResult(nullptr),
  lastWorkerStatus(DEAD)
{
  boost::format form("http://%1%:%2%/task");
  form % host % port;
  urlBase = form.str();
}

bool WorkerState::addTask(const WorkerTask& task) {
  const auto requestDto = dto::WorkerLaunchTaskRequest{task.taskPart.hash, task.taskPart.start, task.taskPart.count};

  auto request = httpClient.CreateRequest().url(urlBase + "/launch")
    .post()
    .data(ToString( userver::formats::json::ValueBuilder(requestDto).ExtractValue()))
    .retry(0)
    .timeout(1000);

  try {
    const auto res = request.perform();
    if (!res->IsOk()) {
      lastWorkerStatus = DEAD;
      workerTask = nullptr;
      return false;
    }

    const auto resJson =  userver::formats::json::FromString(res->body_view());

    auto resDto = resJson.As<dto::WorkerLaunchTaskResponse>();

    workerTaskId = std::make_unique<WorkerTaskId>(resDto.task_id);
    lastWorkerStatus = IN_PROGRESS;
    workerTask = std::make_unique<WorkerTask>(task);
  }
  catch (const userver::clients::http::BaseException& ex) {
    LOG_WARNING() << "can't send request to worker " << urlBase << " what: " << ex.what();
    lastWorkerStatus = DEAD;
    workerTask = nullptr;
  }

  return true;
}


void WorkerState::cancelTask() {
  const auto requestDto = dto::WorkerKillTaskRequest{*workerTaskId};

  auto request = httpClient.CreateRequest().url(urlBase + "/kill")
    .post()
    .data(ToString( userver::formats::json::ValueBuilder(requestDto).ExtractValue()))
    .retry(0)
    .timeout(1000);

  try {
    const auto res = request.perform();
    if (!res->IsOk()) {
      return;
    }

    const auto resJson =  userver::formats::json::FromString(res->body_view());

    auto resDto = resJson.As<dto::DetailedResponse>();

    lastTaskResult = nullptr;
    lastWorkerStatus = DONE;
  }
  catch (const userver::clients::http::BaseException& ex) {
    LOG_WARNING() << "can't send request to worker " << urlBase << " what: " << ex.what();
    lastWorkerStatus = DEAD;
  }
}


void WorkerState::updateStatus() {
  const auto requestDto = dto::WorkerStatusTaskRequest{*workerTaskId};

  auto request = httpClient.CreateRequest().url(urlBase + "/status")
    .post()
    .data(ToString( userver::formats::json::ValueBuilder(requestDto).ExtractValue()))
    .retry(0)
    .timeout(1000);

  try {
    const auto res = request.perform();
    if (!res->IsOk()) {
      lastWorkerStatus = DEAD;
      return;
    }

    const auto resJson = userver::formats::json::FromString(res->body_view());

    auto resDto = resJson.As<dto::WorkerStatusTaskResponse>();

    switch (task::TaskResultTypeFromString(resDto.status)) {
      case task::IN_PROGRESS:
        lastWorkerStatus = IN_PROGRESS;
        lastTaskResult = nullptr;
        break;
      case task::FOUND:
        lastWorkerStatus = DONE;
        lastTaskResult =
            std::make_unique<std::string>(resDto.result.value());
        break;
      case task::NOT_FOUND:
      case task::CANCELED:
        lastWorkerStatus = DONE;
        lastTaskResult = nullptr;
        break;
      case task::NO_SUCH_TASK:
        lastWorkerStatus = DEAD;
        lastTaskResult = nullptr;
        workerTaskId = nullptr;
        break;
    }
  } catch (const userver::clients::http::BaseException& ex) {
    LOG_WARNING() << "can't send request to worker " << urlBase
                  << " what: " << ex.what();
    lastWorkerStatus = DEAD;
    lastTaskResult = nullptr;
  }

}

} // manager