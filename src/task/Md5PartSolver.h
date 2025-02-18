#ifndef MD5PARTSOLVER_H
#define MD5PARTSOLVER_H

#include <string>
#include <cstdint>
#include <utility>
#include <vector>

namespace task {

struct Md5Part{
  std::string hash;
  std::string start;
  std::int64_t count;
  Md5Part(std::string  hash, std::string  start, std::int64_t count)
      : hash(std::move(hash)), start(std::move(start)), count(count) {}
};

std::vector<Md5Part> makeMd5Parts(const std::string& hash, std::int64_t len);

class Md5PartSolver{
  public:
  explicit Md5PartSolver(Md5Part&& part);

  bool solve();
  void cancel() { mCancelled = true; }
  [[nodiscard]] bool isCancelled() const { return mCancelled; }
  [[nodiscard]] const std::string& result() const {return result_;}

  private:
  std::string mHash;
  std::string mCurrent;
  std::int64_t mCount;
  std::string result_;
  bool mCancelled;
};

} // task

#endif //MD5PARTSOLVER_H
