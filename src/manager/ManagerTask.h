//
// Created by user on 3/4/25.
//

#ifndef MANAGERTASK_H
#define MANAGERTASK_H

#include <boost/uuid/uuid.hpp>
#include "task/Md5PartSolver.h"
#include "userver/utils/boost_uuid4.hpp"
#include "userver/utils/trivial_map.hpp"

namespace manager {
enum ManagerTaskResultType{FOUND, IN_PROGRESS, NOT_FOUND};

constexpr userver::utils::TrivialBiMap managerTaskResultMap = [](auto selector) {
  return selector().Case("IN_PROGRESS", IN_PROGRESS)
      .Case("FOUND", FOUND)
      .Case("NOT_FOUND", NOT_FOUND);
};

inline std::string_view ManagerTaskResultTypeToString(const ManagerTaskResultType type) {
  return managerTaskResultMap.TryFind(type).value();
}

inline ManagerTaskResultType ManagerTaskResultTypeFromString(const std::string_view& string) {
  return managerTaskResultMap.TryFind(string).value();
}

struct ManagerTaskResult {
  std::string result;
  ManagerTaskResultType type;

  ManagerTaskResult(): type(IN_PROGRESS){};
};

class ManagerTask {
public:
  using TaskId = boost::uuids::uuid;

  ManagerTask(const std::string& hash, std::int64_t len):
    id(userver::utils::generators::GenerateBoostUuid()),
    partMaker(hash, len)
  {}

  ManagerTask() : id() {};

  [[nodiscard]] const TaskId& getId() const { return id;}

  [[nodiscard]] task::Md5Part getNextPart() {
    return partMaker.nextPart();
  }

  [[nodiscard]] bool isDone() const {
    return partMaker.isDone();
  }

  [[nodiscard]] bool isValid() const {
    return partMaker.isValid();
  }
private:
  TaskId id;
  task::Md5PartMaker partMaker;
};

struct WorkerTask {
  ManagerTask::TaskId id{};
  task::Md5Part taskPart;
};




}
#endif //MANAGERTASK_H
