#include "../task/Md5PartSolver.h"

#include <userver/crypto/hash.hpp>
#include <userver/utest/utest.hpp>

#include "userver/drivers/impl/connection_pool_base.hpp"

static const std::string hash_2 = userver::crypto::hash::weak::Md5("12");

static const std::string hash_3 = userver::crypto::hash::weak::Md5("123");

static const std::string hash_4 = userver::crypto::hash::weak::Md5("1234");

static const std::string hash_4_abc = userver::crypto::hash::weak::Md5("abAB");


using namespace task;

UTEST(test_generate_md5_task_parts, task_2) {
  auto tasks = makeMd5Parts(hash_2, 2);

  ASSERT_EQ(tasks.size(), 1);

  for (auto& task: tasks) {
    Md5PartSolver solver(std::move(task));
    auto res = solver.solve();
    ASSERT_EQ(res, true);
    ASSERT_EQ(solver.result(), "12");
  }
}

UTEST(test_generate_md5_task_parts, tast_3) {
  auto tasks = makeMd5Parts(hash_3, 3);

  ASSERT_EQ(tasks.size(), 1);

  for (auto& task: tasks) {
    Md5PartSolver solver(std::move(task));
    auto res = solver.solve();
    ASSERT_EQ(res, true);
    ASSERT_EQ(solver.result(), "123");
  }

}

UTEST(test_generate_md5_task_parts, tast_4) {
  auto tasks = makeMd5Parts(hash_4, 4);

  ASSERT_EQ(tasks.size(), 62);

  std::string result;
  for (auto& task: tasks) {
    Md5PartSolver solver(std::move(task));
    auto res = solver.solve();
    if (res) {
      result = solver.result();
    }

  }
  ASSERT_EQ(result, "1234");
}

UTEST(test_generate_md5_task_parts, tast_4_ab) {
  auto tasks = makeMd5Parts(hash_4_abc, 4);

  ASSERT_EQ(tasks.size(), 62);

  std::string result;
  for (auto& task: tasks) {
    Md5PartSolver solver(std::move(task));
    auto res = solver.solve();
    if (res) {
      result = solver.result();
    }

  }
  ASSERT_EQ(result, "abAB");
}

