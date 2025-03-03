#include <unicode/urename.h>

#include "task/Md5PartSolver.h"

#include <userver/crypto/hash.hpp>
#include <userver/utest/utest.hpp>

static const std::string hash_2 = userver::crypto::hash::weak::Md5("12");

static const std::string hash_3 = userver::crypto::hash::weak::Md5("123");

static const std::string hash_4 = userver::crypto::hash::weak::Md5("1234");

static const std::string hash_4_abc = userver::crypto::hash::weak::Md5("abAB");

static const std::string hash_start = userver::crypto::hash::weak::Md5("aa");

static const std::string hash_end = userver::crypto::hash::weak::Md5("00");

static const std::string hash_start_4 = userver::crypto::hash::weak::Md5("aaaa");

static const std::string hash_end_4 = userver::crypto::hash::weak::Md5("0000");

static const std::string hash_large = userver::crypto::hash::weak::Md5("aaaaaaa");

using namespace task;


static void generic_md5_task_test(const std::string& hash, const std::string& expect,
                                  const std::size_t expect_size) {
  auto tasks = makeMd5Parts(hash, static_cast<std::int64_t>(expect.length()));

  ASSERT_EQ(tasks.size(), expect_size);

  std::string result;
  for (auto& task: tasks) {
    Md5PartSolver solver(task);
    auto res = solver.solve();
    if (res) {
      result = solver.result();
      break;
    }
  }
  ASSERT_EQ(result, expect);
}


UTEST(test_generate_md5_task_parts, test_2) {
  generic_md5_task_test(hash_2, "12", 1);
}


UTEST(test_generate_md5_task_parts, test_3) {
  generic_md5_task_test(hash_3, "123", 1);
}


UTEST(test_generate_md5_task_parts, test_4) {
  generic_md5_task_test(hash_4, "1234", 62);
}


UTEST(test_generate_md5_task_parts, test_4_ab) {
  generic_md5_task_test(hash_4_abc, "abAB", 62);
}


UTEST(test_generate_md5_task_parts, test_start) {
  generic_md5_task_test(hash_start, "aa", 1);
}

UTEST(test_generate_md5_task_parts, test_start_4) {
  generic_md5_task_test(hash_start_4, "aaaa", 62);
}

UTEST(test_generate_md5_task_parts, test_end) {
  generic_md5_task_test(hash_end, "00", 1);
}

UTEST(test_generate_md5_task_parts, test_end_4) {
  generic_md5_task_test(hash_end_4, "0000", 62);
}

UTEST(test_generate_md5_task_parts, test_large) {
  generic_md5_task_test(hash_large, "aaaaaaa", 14776336);
}