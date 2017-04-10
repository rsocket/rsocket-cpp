// Copyright 2004-present Facebook. All Rights Reserved.

#include "rsocket/RSocketRequester.h"

#include "rsocket/OldNewBridge.h"

#include <folly/ExceptionWrapper.h>

using namespace reactivesocket;
using namespace folly;

namespace rsocket {

std::shared_ptr<RSocketRequester> RSocketRequester::create(
    std::unique_ptr<StandardReactiveSocket> srs,
    EventBase& eventBase) {
  auto customDeleter = [&eventBase](RSocketRequester* pRequester) {
    eventBase.runImmediatelyOrRunInEventBaseThreadAndWait([&pRequester] {
      LOG(INFO) << "RSocketRequester => destroy on EventBase";
      delete pRequester;
    });
  };

  auto* rsr = new RSocketRequester(std::move(srs), eventBase);
  std::shared_ptr<RSocketRequester> sR(rsr, customDeleter);
  return sR;
}

RSocketRequester::RSocketRequester(
    std::unique_ptr<StandardReactiveSocket> srs,
    EventBase& eventBase)
    : standardReactiveSocket_(std::move(srs)), eventBase_(eventBase) {}

RSocketRequester::~RSocketRequester() {
  LOG(INFO) << "RSocketRequester => destroy";
}

std::shared_ptr<Subscriber<Payload>> RSocketRequester::requestChannel(
    std::shared_ptr<Subscriber<Payload>> responseSink) {
  // TODO need to runInEventBaseThread like other request methods
  return standardReactiveSocket_->requestChannel(std::move(responseSink));
}

void RSocketRequester::requestStream(
    Payload request,
    std::shared_ptr<Subscriber<Payload>> responseSink) {
  eventBase_.runInEventBaseThread(
      [ this, request = std::move(request), responseSink ]() mutable {
        standardReactiveSocket_->requestStream(
            std::move(request), std::move(responseSink));
      });
}

std::shared_ptr<yarpl::flowable::Flowable<reactivesocket::Payload>>
RSocketRequester::requestStream(reactivesocket::Payload request) {
  auto& eb = eventBase_;
  auto srs = standardReactiveSocket_;
  return yarpl::flowable::Flowable<reactivesocket::Payload>::create(
      [&eb, request = std::move(request), srs = std::move(srs) ](
          auto uptr_subscriber) mutable {
        auto os =
            std::make_shared<OldToNewSubscriber>(std::move(uptr_subscriber));
        eb.runInEventBaseThread([
          request = std::move(request),
          os = std::move(os),
          srs = std::move(srs)
        ]() mutable { srs->requestStream(std::move(request), std::move(os)); });
      });
}

void RSocketRequester::requestResponse(
    Payload request,
    std::shared_ptr<Subscriber<Payload>> responseSink) {
  eventBase_.runInEventBaseThread(
      [ this, request = std::move(request), responseSink ]() mutable {
        standardReactiveSocket_->requestResponse(
            std::move(request), std::move(responseSink));
      });
}

void RSocketRequester::requestFireAndForget(Payload request) {
  eventBase_.runInEventBaseThread(
      [ this, request = std::move(request) ]() mutable {
        standardReactiveSocket_->requestFireAndForget(std::move(request));
      });
}

void RSocketRequester::metadataPush(std::unique_ptr<folly::IOBuf> metadata) {
  eventBase_.runInEventBaseThread(
      [ this, metadata = std::move(metadata) ]() mutable {
        standardReactiveSocket_->metadataPush(std::move(metadata));
      });
}
}
