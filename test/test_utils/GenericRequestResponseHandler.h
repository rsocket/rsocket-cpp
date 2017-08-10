// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

// #include "RSocketTests.h"
#include "yarpl/Single.h"

#include "folly/ExceptionWrapper.h"

using namespace yarpl;
using namespace yarpl::single;
using namespace rsocket;
using namespace rsocket::tests;
using namespace rsocket::tests::client_server;

namespace rsocket {
namespace tests {

using StringPair = std::pair<std::string, std::string>;

inline std::ostream& operator<<(std::ostream& os, StringPair const& payload) {
  return os << "('" << payload.first << "', '" << payload.second << "')";
}

struct ResponseImpl {
  enum class Type { PAYLOAD, EXCEPTION };

  StringPair p;
  std::exception_ptr e;
  Type type;

  explicit ResponseImpl(StringPair const& p) : p(p), type(Type::PAYLOAD) {}
  explicit ResponseImpl(std::exception_ptr const& e)
      : e(e), type(Type::EXCEPTION) {}

  ~ResponseImpl() {}
};

using Response = std::unique_ptr<ResponseImpl>;

// Type that maps a request (data/metadata) to a response
// (data/metadata or exception)
using HandlerFunc = folly::Function<Response(StringPair const&)>;

struct GenericRequestResponseHandler : public rsocket::RSocketResponder {
  GenericRequestResponseHandler(HandlerFunc&& func)
      : handler_(std::make_unique<HandlerFunc>(std::move(func))) {}

  Reference<Single<Payload>> handleRequestResponse(Payload request, StreamId)
      override {
    auto data = request.moveDataToString();
    auto meta = request.moveMetadataToString();

    StringPair req(data, meta);
    Response resp = (*handler_)(req);

    return Single<Payload>::create(
        [ resp = std::move(resp), this ](auto subscriber) {
          subscriber->onSubscribe(SingleSubscriptions::empty());

          if (resp->type == ResponseImpl::Type::PAYLOAD) {
            subscriber->onSuccess(Payload(resp->p.first, resp->p.second));
          } else if (resp->type == ResponseImpl::Type::EXCEPTION) {
            subscriber->onError(resp->e);
          } else {
            throw std::runtime_error("unknown response type");
          }
        });
  }

  ~GenericRequestResponseHandler() {}

 private:
  std::unique_ptr<HandlerFunc> handler_;
};

Response payload_response(StringPair const& sp) {
  return std::make_unique<ResponseImpl>(sp);
}

Response payload_response(std::string const& a, std::string const& b) {
  return payload_response({a, b});
}

template <typename T>
Response error_response(T const& err) {
  return std::make_unique<ResponseImpl>(std::make_exception_ptr(err));
}

StringPair payload_to_stringpair(Payload p) {
  return StringPair(p.moveDataToString(), p.moveMetadataToString());
}
}
} /* namespace rsocket::tests */