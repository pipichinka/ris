#include "Md5PartSolver.h"
#include <cmath>
#include <userver/crypto/hash.hpp>

#include "userver/engine/async.hpp"

namespace task{

static constexpr std::string_view digits = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890";

static constexpr std::string_view hash_digits = "0123456789abcdef";

static constexpr std::int64_t digits_len = digits.length();

static constexpr std::int64_t iterationsPerPart = digits_len * digits_len * digits_len; //digits_len^3

static std::int64_t pow(const std::int64_t base, const std::int64_t exp) {
  std::int64_t result = 1;
  for (std::int64_t i = 0; i < exp; ++i) {
    result *= base;
  }
  return result;
}

bool Md5Part::isValid() const {
  bool res;
  res = std::all_of(hash.begin(), hash.end(),
    [] (char digit) { return hash_digits.find(digit) != std::string::npos;} );
  if (!res) {
    return false;
  }
  res = std::all_of(start.begin(), start.end(),
    [] (char digit) { return digits.find(digit) != std::string::npos;} );

  if (!res) {
    return false;
  }
  if (count <= 0) {
    return false;
  }
  return true;
}


std::vector<Md5Part>
makeMd5Parts(const std::string& hash, std::int64_t len){
  std::vector<Md5Part> parts;

  std::vector<std::int64_t> indexString(len);
  for (std::int64_t i = 0; i < len; ++i) {
    indexString[i] = 0;
  }
  while (true) {
    std::int64_t moveToNextDigit = iterationsPerPart;
    std::string curStart;

    for (std::int64_t pos = 0; pos < len; pos++) {
      curStart.push_back(digits[indexString[pos]]);
    }

    parts.emplace_back(hash, curStart, iterationsPerPart);

    for (std::int64_t pos = len -1; pos >= 0; --pos) {
      const std::int64_t curValue = indexString[pos] + moveToNextDigit;
      indexString[pos] = (curValue) % digits_len;
      moveToNextDigit = curValue / digits_len;
      if (moveToNextDigit == 0) {
        break;
      }
    }

    if (moveToNextDigit != 0) {
      break;
    }
  }

  if (len <= 3) {
    parts.end()->count = pow(digits_len, len);
  }

  return parts;
}

Md5PartSolver::Md5PartSolver(Md5Part&& part):
  mHash(std::move(part.hash)),
  mCurrent(std::move(part.start)),
  mCount(part.count),
  mCancelled(false){
}

Md5PartSolver::Md5PartSolver(const Md5Part& part):
  mHash(part.hash),
  mCurrent(part.start),
  mCount(part.count),
  mCancelled(false){
}



bool Md5PartSolver::solve() {
  std::vector<std::int64_t> indexString;
  for (char i : mCurrent) {
    indexString.push_back(static_cast<std::int64_t>(digits.find(i)));
  }

  for (std::int64_t i = 0; i < mCount; ++i) {
    std::int64_t moveToNextDigit = 1;

    if (userver::crypto::hash::weak::Md5(mCurrent) == mHash) {
      result_ = mCurrent;
      return true;
    }

    if (i % 100 == 0 && userver::engine::current_task::ShouldCancel()) {
      mCancelled = true;
      return false;
    }

    for (auto pos = static_cast<std::int64_t>( mCurrent.size() - 1); pos >= 0; --pos) {
      const std::int64_t curValue = indexString[pos] + moveToNextDigit;
      indexString[pos] = (curValue) % digits_len;
      mCurrent[pos] = digits[indexString[pos]];
      moveToNextDigit = curValue / digits_len;
      if (moveToNextDigit == 0) {
        break;
      }
    }
  }

  return false;
}

} // task