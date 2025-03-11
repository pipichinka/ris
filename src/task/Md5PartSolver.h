#ifndef MD5PARTSOLVER_H
#define MD5PARTSOLVER_H

#include <string>
#include <cstdint>
#include <utility>
#include <vector>
#include <iostream>

namespace task {

struct Md5Part{
  std::string hash;
  std::string start;
  std::int64_t count;
  Md5Part(std::string  hash, std::string  start, std::int64_t count)
      : hash(std::move(hash)), start(std::move(start)), count(count) {}
  [[nodiscard]] bool isValid() const;
};

inline std::ostream& operator<<(std::ostream& s, const Md5Part& t) {
  s << "hash: " << t.hash << ", start: " << t.start << ", count: " << t.count;
  return s;
}

std::vector<Md5Part> makeMd5Parts(const std::string& hash, std::int64_t len);


class Md5PartMaker {
public:
  Md5PartMaker(std::string hash, std::int64_t len);
  Md5PartMaker():done(true) {}
  Md5Part nextPart();
  [[nodiscard]] bool isValid() const;
  [[nodiscard]] bool isDone() const { return done;}
private:
  std::string hash;
  std::vector<std::int64_t> indexString;
  bool done;
};


class Md5PartSolver{
  public:
  explicit Md5PartSolver(Md5Part&& part);
  explicit Md5PartSolver(const Md5Part& part);

  bool solve();
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
