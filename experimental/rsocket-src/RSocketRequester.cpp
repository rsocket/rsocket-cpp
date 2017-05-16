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
  return yarpl::flowable::Flowables::fromPublisher<Payload>([
    &eb,
    requestStream = std::move(requestStream),
    srs = std::move(srs)
  ](yarpl::Reference<yarpl::flowable::Subscriber<Payload>> subscriber) mutable {
    // TODO eliminate OldToNew bridge
    auto os = std::make_shared<OldToNewSubscriber>(std::move(subscriber));
    eb.runInEventBaseThread([
      requestStream = std::move(requestStream),
      os = std::move(os),
      srs = std::move(srs)
    ]() mutable {
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

yarpl::Reference<yarpl::single::Single<reactivesocket::Payload>>
RSocketRequester::requestResponse(Payload request) {
  // TODO bridge in use until SingleSubscriber is used internally
  class SingleToSubscriberBridge : public Subscriber<Payload> {
   public:
    SingleToSubscriberBridge(
        yarpl::Reference<yarpl::single::SingleObserver<Payload>>
            singleSubscriber)
        : singleSubscriber_{std::move(singleSubscriber)} {}

    void onSubscribe(
        std::shared_ptr<Subscription> subscription) noexcept override {
      // register cancellation callback with SingleSubscriber
      auto singleSubscription = yarpl::single::SingleSubscriptions::create(
          [subscription] { subscription->cancel(); });

      singleSubscriber_->onSubscribe(std::move(singleSubscription));

      // kick off request (TODO this is not needed once we use the proper type)
      subscription->request(1);
    }
    void onNext(Payload payload) noexcept override {
      singleSubscriber_->onSuccess(std::move(payload));
    }
    void onComplete() noexcept override {
      // ignore as we're done once we get a single value back
    }
    void onError(folly::exception_wrapper ex) noexcept override {
      LOG(ERROR) << ex.what();
      // TODO what happens if it doesn't have an exception_ptr?
      // TODO we are eliminating this bridge within 48 hours so probably don't
      // need to care
      singleSubscriber_->onError(ex.to_exception_ptr());
    }

   private:
    yarpl::Reference<yarpl::single::SingleObserver<Payload>> singleSubscriber_;
  };

  return yarpl::single::Single<Payload>::create(
      [ eb = &eventBase_, request = std::move(request), srs = reactiveSocket_ ](
          yarpl::Reference<yarpl::single::SingleObserver<Payload>>
              subscriber) mutable {
        eb->runInEventBaseThread([
          request = std::move(request),
          subscriber = std::move(subscriber),
          srs = std::move(srs)
        ]() mutable {
          srs->requestResponse(
              std::move(request),
              std::make_shared<SingleToSubscriberBridge>(
                  std::move(subscriber)));
        });
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
