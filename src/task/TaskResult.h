//
// Created by user on 3/11/25.
//

#ifndef TASKRESULT_H
#define TASKRESULT_H

#include "task/Md5PartSolver.h"
#include "userver/utils/trivial_map.hpp"

namespace task{

enum TaskResultType {IN_PROGRESS, FOUND, NOT_FOUND, CANCELED, NO_SUCH_TASK};

constexpr userver::utils::TrivialBiMap taskResultTypeMap = [](auto selector) {
  return selector().Case("IN_PROGRESS", 0)
      .Case("FOUND", 1)
      .Case("NOT_FOUND", 2)
      .Case("CANCELED", 3)
      .Case("NO_SUCH_TASK", 4);
};

inline std::string_view TaskResultTypeToString(const TaskResultType type) {
  return taskResultTypeMap.TryFind(type).value();
}

inline TaskResultType TaskResultTypeFromString(const std::string_view& string) {
  return static_cast<TaskResultType>(taskResultTypeMap.TryFind(string).value_or(-1));
}

struct TaskResult {
  std::string result{};
  TaskResultType type;
  TaskResult(): type(IN_PROGRESS){}
};

inline std::ostream& operator<<(std::ostream& s, const TaskResult& t) {
  s << "type: " << TaskResultTypeToString(t.type) << ". result: " << t.result;
  return s;
}

}//task
#endif //TASKRESULT_H
