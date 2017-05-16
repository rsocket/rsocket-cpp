// Copyright 2004-present Facebook. All Rights Reserved.

#include "RSocketRequester.h"

#include "src/temporary_home/OldNewBridge.h"
#include "yarpl/Flowable.h"

#include <folly/ExceptionWrapper.h>

using namespace reactivesocket;
using namespace folly;
using namespace yarpl;

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
    eb.runInEventBaseThread([
      requestStream = std::move(requestStream),
      subscriber = std::move(subscriber),
      srs = std::move(srs)
    ]() mutable {
      auto responseSink = srs->requestChannel(std::move(subscriber));
      requestStream->subscribe(std::move(responseSink));
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
    eb->runInEventBaseThread([
      request = std::move(request),
      subscriber = std::move(subscriber),
      srs = std::move(srs)
    ]() mutable { srs->requestStream(std::move(request), std::move(subscriber)); });
  });
}

yarpl::Reference<yarpl::single::Single<reactivesocket::Payload>>
RSocketRequester::requestResponse(Payload request) {
  // TODO bridge in use until SingleSubscriber is used internally
  class SingleToSubscriberBridge : public yarpl::flowable::Subscriber<Payload> {
   public:
    SingleToSubscriberBridge(
        yarpl::Reference<yarpl::single::SingleObserver<Payload>>
            singleSubscriber)
        : singleSubscriber_{std::move(singleSubscriber)} {}

    void onSubscribe(
        yarpl::Reference<yarpl::flowable::Subscription> subscription) noexcept override {
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
    void onError(std::exception_ptr ex) noexcept override {
      DLOG(ERROR) << folly::exceptionStr(ex);
      singleSubscriber_->onError(std::move(ex));
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
              make_ref<SingleToSubscriberBridge>(
                  std::move(subscriber)));
        });
      });
}

yarpl::Reference<yarpl::single::Single<void>> RSocketRequester::fireAndForget(
    reactivesocket::Payload request) {
  return yarpl::single::Single<void>::create([
    eb = &eventBase_,
    request = std::move(request),
    srs = reactiveSocket_
  ](yarpl::Reference<yarpl::single::SingleObserver<void>> subscriber) mutable {
    eb->runInEventBaseThread([
      request = std::move(request),
      subscriber = std::move(subscriber),
      srs = std::move(srs)
    ]() mutable {
      // TODO pass in SingleSubscriber for underlying layers to
      // call onSuccess/onError once put on network
      srs->requestFireAndForget(std::move(request));
      // right now just immediately call onSuccess
      subscriber->onSuccess();
    });
  });
}

void RSocketRequester::metadataPush(std::unique_ptr<folly::IOBuf> metadata) {
  eventBase_.runInEventBaseThread(
      [ this, metadata = std::move(metadata) ]() mutable {
        reactiveSocket_->metadataPush(std::move(metadata));
      });
}
}
