// Copyright 2004-present Facebook. All Rights Reserved.

#include "rsocket/RSocketRequester.h"

#include "rsocket/OldNewBridge.h"
#include "yarpl/Flowable.h"

#include <folly/ExceptionWrapper.h>

using namespace reactivesocket;
using namespace folly;

namespace rsocket {

std::shared_ptr<RSocketRequester> RSocketRequester::create(
    std::unique_ptr<ReactiveSocket> srs,
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
    std::unique_ptr<ReactiveSocket> srs,
    EventBase& eventBase)
    : reactiveSocket_(std::move(srs)), eventBase_(eventBase) {}

RSocketRequester::~RSocketRequester() {
  LOG(INFO) << "RSocketRequester => destroy";
}

yarpl::Reference<yarpl::flowable::Flowable<reactivesocket::Payload>>
RSocketRequester::requestChannel(
    yarpl::Reference<yarpl::flowable::Flowable<reactivesocket::Payload>>
    requestStream) {
  auto& eb = eventBase_;
  auto srs = reactiveSocket_;

  LOG(INFO) << "requestChannel executing ";

  return yarpl::flowable::Flowables::fromPublisher<Payload>([
    &eb,
    requestStream = std::move(requestStream),
    srs = std::move(srs)
  ](yarpl::Reference<yarpl::flowable::Subscriber<Payload>> subscriber) mutable {
    // TODO eliminate OldToNew bridge
    auto os = std::make_shared<OldToNewSubscriber>(std::move(subscriber));

    LOG(INFO) << "requestChannel ABOUT TO RUN ON THREAD ";

    eb.runInEventBaseThread([
      requestStream = std::move(requestStream),
      os = std::move(os),
      srs = std::move(srs)
    ]() mutable {

        LOG(INFO) << "requestChannel RUNNING IN THREAD";

      auto responseSink = srs->requestChannel(std::move(os));
      // TODO eliminate NewToOld bridge
      requestStream->subscribe(
          yarpl::make_ref<NewToOldSubscriber>(std::move(responseSink)));
    });
  });
}

yarpl::Reference<yarpl::flowable::Flowable<Payload>>
RSocketRequester::requestStream(Payload request) {
  return yarpl::flowable::Flowables::fromPublisher<Payload>([
    eb = &eventBase_,
    request = std::move(request),
    srs = reactiveSocket_
  ](yarpl::Reference<yarpl::flowable::Subscriber<Payload>> subscriber) mutable {
    // TODO eliminate OldToNew bridge
    auto os = std::make_shared<OldToNewSubscriber>(std::move(subscriber));
    eb->runInEventBaseThread([
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
        reactiveSocket_->requestResponse(
            std::move(request), std::move(responseSink));
      });
}

void RSocketRequester::requestFireAndForget(Payload request) {
  eventBase_.runInEventBaseThread(
      [ this, request = std::move(request) ]() mutable {
        reactiveSocket_->requestFireAndForget(std::move(request));
      });
}

void RSocketRequester::metadataPush(std::unique_ptr<folly::IOBuf> metadata) {
  eventBase_.runInEventBaseThread(
      [ this, metadata = std::move(metadata) ]() mutable {
        reactiveSocket_->metadataPush(std::move(metadata));
      });
}
}
