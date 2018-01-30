// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include "yarpl/Single.h"

#include "folly/ExceptionWrapper.h"

namespace rsocket {
namespace tests {

using StringPair = std::pair<std::string, std::string>;

struct ResponseImpl {
  enum class Type { PAYLOAD, EXCEPTION };

  StringPair p;
  folly::exception_wrapper e;
  Type type;

  explicit ResponseImpl(StringPair const& p) : p(p), type(Type::PAYLOAD) {}
  explicit ResponseImpl(folly::exception_wrapper e)
      : e(std::move(e)), type(Type::EXCEPTION) {}

  ~ResponseImpl() {}
};

using Response = std::unique_ptr<ResponseImpl>;

// Type that maps a request (data/metadata) to a response
// (data/metadata or exception)
using HandlerFunc = folly::Function<Response(StringPair const&)>;

struct GenericRequestResponseHandler : public rsocket::RSocketResponder {
  explicit GenericRequestResponseHandler(HandlerFunc&& func)
      : handler_(std::make_unique<HandlerFunc>(std::move(func))) {}

  std::shared_ptr<yarpl::single::Single<Payload>> handleRequestResponse(
      Payload request,
      StreamId) override {
    auto ioBufChainToString = [](std::unique_ptr<folly::IOBuf> buf) {
      folly::IOBufQueue queue;
      queue.append(std::move(buf));

      std::string ret;
      while (auto elem = queue.pop_front()) {
        auto part = elem->moveToFbString();
        ret += part.toStdString();
      }

      return ret;
    };

    std::string data = ioBufChainToString(std::move(request.data));
    std::string meta = ioBufChainToString(std::move(request.metadata));

    StringPair req(data, meta);
    Response resp = (*handler_)(req);

    return yarpl::single::Single<Payload>::create(
        [ resp = std::move(resp), this ](auto subscriber) {
          subscriber->onSubscribe(yarpl::single::SingleSubscriptions::empty());

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

inline Response payload_response(StringPair const& sp) {
  return std::make_unique<ResponseImpl>(sp);
}

inline Response payload_response(std::string const& a, std::string const& b) {
  return payload_response({a, b});
}

template <typename T>
Response error_response(T const& err) {
  return std::make_unique<ResponseImpl>(err);
}

inline StringPair payload_to_stringpair(Payload p) {
  return StringPair(p.moveDataToString(), p.moveMetadataToString());
}
}
}

inline std::ostream& operator<<(
    std::ostream& os,
    rsocket::tests::StringPair const& payload) {
  return os << "('" << payload.first << "', '" << payload.second << "')";
}
