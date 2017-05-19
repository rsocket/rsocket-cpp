// Copyright 2004-present Facebook. All Rights Reserved.

#include "RSocketResponder.h"
#include <folly/io/async/EventBase.h>

namespace rsocket {

yarpl::Reference<yarpl::single::Single<rsocket::Payload>>
RSocketResponder::handleRequestResponse(rsocket::Payload request, rsocket::StreamId streamId) {
  return yarpl::single::Singles::error<rsocket::Payload>(
      std::logic_error("handleRequestResponse not implemented"));
}

yarpl::Reference<yarpl::flowable::Flowable<rsocket::Payload>>
RSocketResponder::handleRequestStream(rsocket::Payload request, rsocket::StreamId streamId) {
  return yarpl::flowable::Flowables::error<rsocket::Payload>(
      std::logic_error("handleRequestStream not implemented"));
}

yarpl::Reference<yarpl::flowable::Flowable<rsocket::Payload>>
RSocketResponder::handleRequestChannel(
    rsocket::Payload request,
    yarpl::Reference<yarpl::flowable::Flowable<rsocket::Payload>>
        requestStream,
    rsocket::StreamId streamId) {
  return yarpl::flowable::Flowables::error<rsocket::Payload>(
      std::logic_error("handleRequestChannel not implemented"));
}

void RSocketResponder::handleFireAndForget(
    rsocket::Payload request,
    rsocket::StreamId streamId) {
  // no default implementation, no error response to provide
}

void RSocketResponder::handleMetadataPush(std::unique_ptr<folly::IOBuf> metadata) {
  // no default implementation, no error response to provide
}

/// Handles a new Channel requested by the other end.
yarpl::Reference<yarpl::flowable::Subscriber<Payload>>
RSocketResponder::handleRequestChannelCore(
    Payload request,
    StreamId streamId,
    const yarpl::Reference<yarpl::flowable::Subscriber<Payload>>&
    response) noexcept {
  class EagerSubscriberBridge
      : public yarpl::flowable::Subscriber<rsocket::Payload> {
   public:
    void onSubscribe(
        yarpl::Reference<yarpl::flowable::Subscription> subscription) noexcept {
      CHECK(!subscription_);
      subscription_ = std::move(subscription);
      if (inner_) {
        inner_->onSubscribe(subscription_);
      }
    }

    void onNext(rsocket::Payload element) noexcept {
      DCHECK(inner_);
      inner_->onNext(std::move(element));
    }

    void onComplete() noexcept {
      DCHECK(inner_);
      inner_->onComplete();

      inner_.reset();
      subscription_.reset();
    }

    void onError(std::exception_ptr ex) noexcept {
      DCHECK(inner_);
      inner_->onError(std::move(ex));

      inner_.reset();
      subscription_.reset();
    }

    void subscribe(
        yarpl::Reference<yarpl::flowable::Subscriber<rsocket::Payload>> inner) {
      CHECK(!inner_); // only one call to subscribe is supported
      CHECK(inner);
      inner_ = std::move(inner);
      if (subscription_) {
        inner_->onSubscribe(subscription_);
      }
    }

   private:
    yarpl::Reference<yarpl::flowable::Subscriber<rsocket::Payload>> inner_;
    yarpl::Reference<yarpl::flowable::Subscription> subscription_;
  };

  auto eagerSubscriber = yarpl::make_ref<EagerSubscriberBridge>();
  auto flowable = handleRequestChannel(
      std::move(request),
      yarpl::flowable::Flowables::fromPublisher<Payload>([eagerSubscriber](
          yarpl::Reference<yarpl::flowable::Subscriber<Payload>> subscriber) {
        eagerSubscriber->subscribe(subscriber);
      }),
      std::move(streamId));
  // bridge from the existing eager RequestHandler and old Subscriber type
  // to the lazy Flowable and new Subscriber type
  flowable->subscribe(std::move(response));
  return eagerSubscriber;
}

/// Handles a new Stream requested by the other end.
void RSocketResponder::handleRequestStreamCore(
    Payload request,
    StreamId streamId,
    const yarpl::Reference<yarpl::flowable::Subscriber<Payload>>&
    response) noexcept {
  auto flowable = handleRequestStream(std::move(request), std::move(streamId));
  flowable->subscribe(std::move(response));
}

/// Handles a new inbound RequestResponse requested by the other end.
void RSocketResponder::handleRequestResponseCore(
    Payload request,
    StreamId streamId,
    const yarpl::Reference<yarpl::flowable::Subscriber<Payload>>&
    responseSubscriber) noexcept {
  auto single = handleRequestResponse(std::move(request), streamId);
  // bridge from the existing eager RequestHandler and old Subscriber type
  // to the lazy Single and new SingleObserver type

  class BridgeSubscriptionToSingle : public yarpl::flowable::Subscription {
   public:
    BridgeSubscriptionToSingle(
        yarpl::Reference<yarpl::single::Single<Payload>> single,
        yarpl::Reference<yarpl::flowable::Subscriber<Payload>>
        responseSubscriber)
        : single_{std::move(single)},
          responseSubscriber_{std::move(responseSubscriber)} {}

    void request(int64_t n) noexcept override {
      // when we get a request we subscribe to Single
      bool expected = false;
      if (n > 0 && subscribed_.compare_exchange_strong(expected, true)) {
        single_->subscribe(yarpl::single::SingleObservers::create<Payload>(
            [this](Payload p) {
              // onNext
              responseSubscriber_->onNext(std::move(p));
            },
            [this](std::exception_ptr eptr) {
              responseSubscriber_->onError(std::move(eptr));
            }));
      }
    }

    void cancel() noexcept override{
      // TODO this code will be deleted shortly, so not bothering to make
      // work
    };

   private:
    yarpl::Reference<yarpl::single::Single<Payload>> single_;
    yarpl::Reference<yarpl::flowable::Subscriber<Payload>>
        responseSubscriber_;
    std::atomic_bool subscribed_{false};
  };

  responseSubscriber->onSubscribe(yarpl::make_ref<BridgeSubscriptionToSingle>(
      std::move(single), responseSubscriber));
}
}
