//
// Created by user on 3/11/25.
//

#ifndef HANDLERUTILS_H
#define HANDLERUTILS_H
#include <userver/server/handlers/exceptions.hpp>

#include "src/dto/jeneral.hpp"

class DetailedErrorBuilder {
public:
  static constexpr bool kIsExternalBodyFormatted = true;

  explicit DetailedErrorBuilder(std::string msg) {
    const dto::DetailedResponse response{.detail = std::move(msg)};
    body = ToString(userver::formats::json::ValueBuilder(response).ExtractValue());
  }

  [[nodiscard]] std::string GetExternalBody() const { return body; }

private:
  std::string body;
};

template<class T>
T parseJson(const userver::formats::json::Value& json){
  try{
    return json.As<T>();
  }
  catch(const userver::formats::json::Exception& ex) {
    throw userver::server::handlers::ClientError(
    DetailedErrorBuilder{ex.what()});
  }
}
#endif //HANDLERUTILS_H
