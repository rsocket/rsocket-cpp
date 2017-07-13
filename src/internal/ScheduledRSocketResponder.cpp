// Copyright 2004-present Facebook. All Rights Reserved.

#include "src/internal/ScheduledRSocketResponder.h"

#include <folly/io/async/EventBase.h>

#include "src/internal/ScheduledSingleObserver.h"
#include "src/internal/ScheduledSubscriber.h"

namespace rsocket {

ScheduledRSocketResponder::ScheduledRSocketResponder(
    std::shared_ptr<RSocketResponder> inner,
    folly::EventBase& eventBase) : inner_(std::move(inner)),
                                   eventBase_(eventBase) {}

yarpl::Reference<yarpl::single::Single<Payload>>
ScheduledRSocketResponder::handleRequestResponse(
    Payload request,
    StreamId streamId) {
  auto innerFlowable = inner_->handleRequestResponse(std::move(request),
                                                     streamId);
  return yarpl::single::Singles::create<Payload>(
  [innerFlowable = std::move(innerFlowable), eventBase = &eventBase_](
      yarpl::Reference<yarpl::single::SingleObserver<Payload>>
  observer) {
    innerFlowable->subscribe(yarpl::make_ref<
        ScheduledSingleObserver<Payload>>
                                 (std::move(observer), *eventBase));
  });
}

yarpl::Reference<yarpl::flowable::Flowable<Payload>>
ScheduledRSocketResponder::handleRequestStream(
    Payload request,
    StreamId streamId) {
  auto innerFlowable = inner_->handleRequestStream(std::move(request),
                                                   streamId);
  return yarpl::flowable::Flowables::fromPublisher<Payload>(
  [innerFlowable = std::move(innerFlowable), eventBase = &eventBase_](
      yarpl::Reference<yarpl::flowable::Subscriber<Payload>>
  subscriber) {
    innerFlowable->subscribe(yarpl::make_ref<
        ScheduledSubscriber<Payload>>
                                 (std::move(subscriber), *eventBase));
  });
}

yarpl::Reference<yarpl::flowable::Flowable<Payload>>
ScheduledRSocketResponder::handleRequestChannel(
    Payload request,
    yarpl::Reference<yarpl::flowable::Flowable<Payload>>
    requestStream,
    StreamId streamId) {
  auto requestStreamFlowable = yarpl::flowable::Flowables::fromPublisher<Payload>(
  [requestStream = std::move(requestStream), eventBase = &eventBase_](
      yarpl::Reference<yarpl::flowable::Subscriber<Payload>>
  subscriber) {
    requestStream->subscribe(yarpl::make_ref<
        ScheduledSubscriptionSubscriber<Payload>>
                                 (std::move(subscriber), *eventBase));
  });
  auto innerFlowable = inner_->handleRequestChannel(std::move(request),
                                                    std::move(
                                                        requestStreamFlowable),
                                                    streamId);
  return yarpl::flowable::Flowables::fromPublisher<Payload>(
  [innerFlowable = std::move(innerFlowable), eventBase = &eventBase_](
      yarpl::Reference<yarpl::flowable::Subscriber<Payload>>
  subscriber) {
    innerFlowable->subscribe(yarpl::make_ref<
        ScheduledSubscriber<Payload>>
                                 (std::move(subscriber), *eventBase));
  });
}

void ScheduledRSocketResponder::handleFireAndForget(
    Payload request,
    StreamId streamId) {
  inner_->handleFireAndForget(std::move(request), streamId);
}

} // rsocket
