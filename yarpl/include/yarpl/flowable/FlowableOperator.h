// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <cassert>
#include <utility>

#include "yarpl/flowable/Flowable.h"
#include "yarpl/flowable/Subscriber.h"
#include "yarpl/flowable/Subscription.h"
#include "yarpl/utils/credits.h"

#include <folly/Executor.h>
#include <folly/functional/Invoke.h>

namespace yarpl {
namespace flowable {

/**
 * Base (helper) class for operators.  Operators are templated on two types: D
 * (downstream) and U (upstream).  Operators are created by method calls on an
 * upstream Flowable, and are Flowables themselves.  Multi-stage pipelines can
 * be built: a Flowable heading a sequence of Operators.
 */
template <typename U, typename D, typename Operator>
class FlowableOperator : public Flowable<D> {
 public:
  explicit FlowableOperator(Reference<Flowable<U>> upstream)
      : upstream_(std::move(upstream)) {}

 protected:
  /// An Operator's subscription.
  ///
  /// When a pipeline chain is active, each Flowable has a corresponding
  /// subscription.  Except for the first one, the subscriptions are created
  /// against Operators.  Each operator subscription has two functions: as a
  /// subscriber for the previous stage; as a subscription for the next one, the
  /// user-supplied subscriber being the last of the pipeline stages.
  class Subscription : public yarpl::flowable::Subscription,
                       public BaseSubscriber<U> {
   protected:
    Subscription(
        Reference<Operator> flowable,
        Reference<Subscriber<D>> subscriber)
        : flowableOperator_(std::move(flowable)),
          subscriber_(std::move(subscriber)) {
      assert(flowableOperator_);
      assert(subscriber_);
    }

    const Reference<Operator>& getFlowableOperator() {
      return flowableOperator_;
    }

    void subscriberOnNext(D value) {
      if (auto subscriber = subscriber_.load()) {
        subscriber->onNext(std::move(value));
      }
    }

    /// Terminates both ends of an operator normally.
    void terminate() {
      auto subscriber = subscriber_.exchange(nullptr);
      BaseSubscriber<U>::cancel();
      if (subscriber) {
        subscriber->onComplete();
      }
    }

    /// Terminates both ends of an operator with an error.
    void terminateErr(folly::exception_wrapper ew) {
      auto subscriber = subscriber_.exchange(nullptr);
      BaseSubscriber<U>::cancel();
      if (subscriber) {
        subscriber->onError(std::move(ew));
      }
    }

    // Subscription.

    void request(int64_t n) override {
      BaseSubscriber<U>::request(n);
    }

    void cancel() override {
      auto subscriber = subscriber_.exchange(nullptr);
      BaseSubscriber<U>::cancel();
    }

    // Subscriber.

    void onSubscribeImpl() override {
      subscriber_->onSubscribe(this->ref_from_this(this));
    }

    void onCompleteImpl() override {
      if (auto subscriber = subscriber_.exchange(nullptr)) {
        subscriber->onComplete();
      }
    }

    void onErrorImpl(folly::exception_wrapper ew) override {
      if (auto subscriber = subscriber_.exchange(nullptr)) {
        subscriber->onError(std::move(ew));
      }
    }

   private:
    /// The Flowable has the lambda, and other creation parameters.
    Reference<Operator> flowableOperator_;

    /// This subscription controls the life-cycle of the subscriber.  The
    /// subscriber is retained as long as calls on it can be made.  (Note: the
    /// subscriber in turn maintains a reference on this subscription object
    /// until cancellation and/or completion.)
    AtomicReference<Subscriber<D>> subscriber_;
  };

  Reference<Flowable<U>> upstream_;
};

template <
    typename U,
    typename D,
    typename F,
    typename = typename std::enable_if<folly::is_invocable_r<D, F, U>::value>::type>
class MapOperator : public FlowableOperator<U, D, MapOperator<U, D, F>> {
  using ThisOperatorT = MapOperator<U, D, F>;
  using Super = FlowableOperator<U, D, ThisOperatorT>;

 public:
  MapOperator(Reference<Flowable<U>> upstream, F function)
      : Super(std::move(upstream)), function_(std::move(function)) {}

  void subscribe(Reference<Subscriber<D>> subscriber) override {
    Super::upstream_->subscribe(make_ref<Subscription>(
        this->ref_from_this(this), std::move(subscriber)));
  }

 private:
  using SuperSubscription = typename Super::Subscription;
  class Subscription : public SuperSubscription {
   public:
    Subscription(
        Reference<ThisOperatorT> flowable,
        Reference<Subscriber<D>> subscriber)
        : SuperSubscription(std::move(flowable), std::move(subscriber)) {}

    void onNextImpl(U value) override {
      try {
        auto&& map = this->getFlowableOperator();
        this->subscriberOnNext(map->function_(std::move(value)));
      } catch (const std::exception& exn) {
        folly::exception_wrapper ew{std::current_exception(), exn};
        this->terminateErr(std::move(ew));
      }
    }
  };

  F function_;
};

template <
    typename U,
    typename F,
    typename =
        typename std::enable_if<folly::is_invocable_r<bool, F, U>::value>::type>
class FilterOperator : public FlowableOperator<U, U, FilterOperator<U, F>> {
  // for use in subclasses
  using ThisOperatorT = FilterOperator<U, F>;
  using Super = FlowableOperator<U, U, ThisOperatorT>;

 public:
  FilterOperator(Reference<Flowable<U>> upstream, F function)
      : Super(std::move(upstream)), function_(std::move(function)) {}

  void subscribe(Reference<Subscriber<U>> subscriber) override {
    Super::upstream_->subscribe(make_ref<Subscription>(
        this->ref_from_this(this), std::move(subscriber)));
  }

 private:
  using SuperSubscription = typename Super::Subscription;
  class Subscription : public SuperSubscription {
   public:
    Subscription(
        Reference<ThisOperatorT> flowable,
        Reference<Subscriber<U>> subscriber)
        : SuperSubscription(std::move(flowable), std::move(subscriber)) {}

    void onNextImpl(U value) override {
      auto&& filter = SuperSubscription::getFlowableOperator();
      if (filter->function_(value)) {
        SuperSubscription::subscriberOnNext(std::move(value));
      } else {
        SuperSubscription::request(1);
      }
    }
  };

  F function_;
};

template <
    typename U,
    typename D,
    typename F,
    typename = typename std::enable_if<std::is_assignable<D, U>::value>,
    typename =
        typename std::enable_if<folly::is_invocable_r<D, F, D, U>::value>::type>
class ReduceOperator : public FlowableOperator<U, D, ReduceOperator<U, D, F>> {
  using ThisOperatorT = ReduceOperator<U, D, F>;
  using Super = FlowableOperator<U, D, ThisOperatorT>;

 public:
  ReduceOperator(Reference<Flowable<U>> upstream, F function)
      : Super(std::move(upstream)), function_(std::move(function)) {}

  void subscribe(Reference<Subscriber<D>> subscriber) override {
    Super::upstream_->subscribe(make_ref<Subscription>(
        this->ref_from_this(this), std::move(subscriber)));
  }

 private:
  using SuperSubscription = typename Super::Subscription;
  class Subscription : public SuperSubscription {
   public:
    Subscription(
        Reference<ThisOperatorT> flowable,
        Reference<Subscriber<D>> subscriber)
        : SuperSubscription(std::move(flowable), std::move(subscriber)),
          accInitialized_(false) {}

    void request(int64_t) override {
      // Request all of the items.
      SuperSubscription::request(credits::kNoFlowControl);
    }

    void onNextImpl(U value) override {
      auto&& reduce = SuperSubscription::getFlowableOperator();
      if (accInitialized_) {
        acc_ = reduce->function_(std::move(acc_), std::move(value));
      } else {
        acc_ = std::move(value);
        accInitialized_ = true;
      }
    }

    void onCompleteImpl() override {
      if (accInitialized_) {
        SuperSubscription::subscriberOnNext(std::move(acc_));
      }
      SuperSubscription::onCompleteImpl();
    }

   private:
    bool accInitialized_;
    D acc_;
  };

  F function_;
};

template <typename T>
class TakeOperator : public FlowableOperator<T, T, TakeOperator<T>> {
  using ThisOperatorT = TakeOperator<T>;
  using Super = FlowableOperator<T, T, ThisOperatorT>;

 public:
  TakeOperator(Reference<Flowable<T>> upstream, int64_t limit)
      : Super(std::move(upstream)), limit_(limit) {}

  void subscribe(Reference<Subscriber<T>> subscriber) override {
    Super::upstream_->subscribe(make_ref<Subscription>(
        this->ref_from_this(this), limit_, std::move(subscriber)));
  }

 private:
  using SuperSubscription = typename Super::Subscription;
  class Subscription : public SuperSubscription {
   public:
    Subscription(
        Reference<ThisOperatorT> flowable,
        int64_t limit,
        Reference<Subscriber<T>> subscriber)
        : SuperSubscription(std::move(flowable), std::move(subscriber)),
          limit_(limit) {}

    void onNextImpl(T value) override {
      if (limit_-- > 0) {
        if (pending_ > 0) {
          --pending_;
        }
        SuperSubscription::subscriberOnNext(std::move(value));
        if (limit_ == 0) {
          SuperSubscription::terminate();
        }
      }
    }

    void request(int64_t delta) override {
      delta = std::min(delta, limit_ - pending_);
      if (delta > 0) {
        pending_ += delta;
        SuperSubscription::request(delta);
      }
    }

   private:
    int64_t pending_{0};
    int64_t limit_;
  };

  const int64_t limit_;
};

template <typename T>
class SkipOperator : public FlowableOperator<T, T, SkipOperator<T>> {
  using ThisOperatorT = SkipOperator<T>;
  using Super = FlowableOperator<T, T, ThisOperatorT>;

 public:
  SkipOperator(Reference<Flowable<T>> upstream, int64_t offset)
      : Super(std::move(upstream)), offset_(offset) {}

  void subscribe(Reference<Subscriber<T>> subscriber) override {
    Super::upstream_->subscribe(make_ref<Subscription>(
        this->ref_from_this(this), offset_, std::move(subscriber)));
  }

 private:
  using SuperSubscription = typename Super::Subscription;
  class Subscription : public SuperSubscription {
   public:
    Subscription(
        Reference<ThisOperatorT> flowable,
        int64_t offset,
        Reference<Subscriber<T>> subscriber)
        : SuperSubscription(std::move(flowable), std::move(subscriber)),
          offset_(offset) {}

    void onNextImpl(T value) override {
      if (offset_ > 0) {
        --offset_;
      } else {
        SuperSubscription::subscriberOnNext(std::move(value));
      }
    }

    void request(int64_t delta) override {
      if (firstRequest_) {
        firstRequest_ = false;
        delta = credits::add(delta, offset_);
      }
      SuperSubscription::request(delta);
    }

   private:
    int64_t offset_;
    bool firstRequest_{true};
  };

  const int64_t offset_;
};

template <typename T>
class IgnoreElementsOperator
    : public FlowableOperator<T, T, IgnoreElementsOperator<T>> {
  using ThisOperatorT = IgnoreElementsOperator<T>;
  using Super = FlowableOperator<T, T, ThisOperatorT>;

 public:
  explicit IgnoreElementsOperator(Reference<Flowable<T>> upstream)
      : Super(std::move(upstream)) {}

  void subscribe(Reference<Subscriber<T>> subscriber) override {
    Super::upstream_->subscribe(make_ref<Subscription>(
        this->ref_from_this(this), std::move(subscriber)));
  }

 private:
  using SuperSubscription = typename Super::Subscription;
  class Subscription : public SuperSubscription {
   public:
    Subscription(
        Reference<ThisOperatorT> flowable,
        Reference<Subscriber<T>> subscriber)
        : SuperSubscription(std::move(flowable), std::move(subscriber)) {}

    void onNextImpl(T) override {}
  };
};

template <typename T>
class SubscribeOnOperator
    : public FlowableOperator<T, T, SubscribeOnOperator<T>> {
  using ThisOperatorT = SubscribeOnOperator<T>;
  using Super = FlowableOperator<T, T, ThisOperatorT>;

 public:
  SubscribeOnOperator(
      Reference<Flowable<T>> upstream,
      folly::Executor& executor)
      : Super(std::move(upstream)), executor_(executor) {}

  void subscribe(Reference<Subscriber<T>> subscriber) override {
    Super::upstream_->subscribe(make_ref<Subscription>(
        this->ref_from_this(this), executor_, std::move(subscriber)));
  }

 private:
  using SuperSubscription = typename Super::Subscription;
  class Subscription : public SuperSubscription {
   public:
    Subscription(
        Reference<ThisOperatorT> flowable,
        folly::Executor& executor,
        Reference<Subscriber<T>> subscriber)
        : SuperSubscription(std::move(flowable), std::move(subscriber)),
          executor_(executor) {}

    void request(int64_t delta) override {
      executor_.add([ delta, this, self = this->ref_from_this(this) ] {
        this->callSuperRequest(delta);
      });
    }

    void cancel() override {
      executor_.add([ this, self = this->ref_from_this(this) ] {
        this->callSuperCancel();
      });
    }

    void onNextImpl(T value) override {
      SuperSubscription::subscriberOnNext(std::move(value));
    }

   private:
    // Trampoline to call superclass method; gcc bug 58972.
    void callSuperRequest(int64_t delta) {
      SuperSubscription::request(delta);
    }

    // Trampoline to call superclass method; gcc bug 58972.
    void callSuperCancel() {
      SuperSubscription::cancel();
    }

    folly::Executor& executor_;
  };

  folly::Executor& executor_;
};

template <typename T, typename OnSubscribe>
class FromPublisherOperator : public Flowable<T> {
 public:
  explicit FromPublisherOperator(OnSubscribe function)
      : function_(std::move(function)) {}

  void subscribe(Reference<Subscriber<T>> subscriber) override {
    function_(std::move(subscriber));
  }

 private:
  OnSubscribe function_;
};

} // namespace flowable
} // namespace yarpl

#include "yarpl/flowable/FlowableObserveOnOperator.h"
