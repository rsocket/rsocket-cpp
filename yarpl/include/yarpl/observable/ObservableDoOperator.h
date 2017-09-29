// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

namespace yarpl {
namespace observable {

template <
    typename U,
    typename OnSubscribeFunc,
    typename OnNextFunc,
    typename OnErrorFunc,
    typename OnCompleteFunc>
class DoOperator : public ObservableOperator<U, U, DoOperator<U, OnSubscribeFunc, OnNextFunc, OnErrorFunc, OnCompleteFunc>> {
  using ThisOperatorT = DoOperator<U, OnSubscribeFunc, OnNextFunc, OnErrorFunc, OnCompleteFunc>;
  using Super = ObservableOperator<U, U, ThisOperatorT>;

 public:
  DoOperator(Reference<Observable<U>> upstream,
             OnSubscribeFunc onSubscribeFunc,
             OnNextFunc onNextFunc,
             OnErrorFunc onErrorFunc,
             OnCompleteFunc onCompleteFunc)
      : Super(std::move(upstream)),
        onSubscribeFunc_(std::move(onSubscribeFunc)),
        onNextFunc_(std::move(onNextFunc)),
        onErrorFunc_(std::move(onErrorFunc)),
        onCompleteFunc_(std::move(onCompleteFunc)) {}

  Reference<Subscription> subscribe(Reference<Observer<U>> observer) override {
    auto subscription =
        make_ref<DoSubscription>(this->ref_from_this(this), std::move(observer));
    Super::upstream_->subscribe(
        // Note: implicit cast to a reference to a observer.
        subscription);
    return subscription;
  }

 private:
  class DoSubscription : public Super::OperatorSubscription {
    using SuperSub = typename Super::OperatorSubscription;

   public:
    DoSubscription(
        Reference<ThisOperatorT> observable,
        Reference<Observer<U>> observer)
        : SuperSub(std::move(observable), std::move(observer)) {}

    void onSubscribe(
        Reference<yarpl::observable::Subscription> subscription) override {
      auto&& op = SuperSub::getObservableOperator();
      op->onSubscribeFunc_();
      SuperSub::onSubscribe(std::move(subscription));
    }

    void onNext(U value) override {
      auto&& op = SuperSub::getObservableOperator();
      const auto& valueRef = value;
      op->onNextFunc_(valueRef);
      SuperSub::observerOnNext(std::move(value));
    }

    void onError(folly::exception_wrapper ex) override {
      auto&& op = SuperSub::getObservableOperator();
      const auto& exRef = ex;
      op->onErrorFunc_(exRef);
      SuperSub::onError(std::move(ex));
    }

    void onComplete() override {
      auto&& op = SuperSub::getObservableOperator();
      op->onCompleteFunc_();
      SuperSub::onComplete();
    }
  };

  OnSubscribeFunc onSubscribeFunc_;
  OnNextFunc onNextFunc_;
  OnErrorFunc onErrorFunc_;
  OnCompleteFunc onCompleteFunc_;
};

namespace details {

template <
    typename U,
    typename OnSubscribeFunc,
    typename OnNextFunc,
    typename OnErrorFunc,
    typename OnCompleteFunc>
inline auto createDoOperator(Reference<Observable<U>> upstream,
                             OnSubscribeFunc onSubscribeFunc,
                             OnNextFunc onNextFunc,
                             OnErrorFunc onErrorFunc,
                             OnCompleteFunc onCompleteFunc) {
  return make_ref<DoOperator<U, OnSubscribeFunc, OnNextFunc, OnErrorFunc, OnCompleteFunc>>(
      std::move(upstream), std::move(onSubscribeFunc), std::move(onNextFunc),
      std::move(onErrorFunc), std::move(onCompleteFunc));
}

}

}
}
