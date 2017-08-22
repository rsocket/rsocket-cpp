// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <memory>
#include <mutex>
#include <utility>

namespace yarpl {
namespace flowable {
namespace details {


template<typename T>
class EmiterBase : public virtual Refcounted {
 public:
  ~EmiterBase() = default;

  virtual std::tuple<int64_t, bool>
  emit(Reference <Subscriber<T>>, int64_t) = 0;
};

/**
 * Manager for a flowable subscription.
 *
 * This is synchronous: the emit calls are triggered within the context
 * of a request(n) call.
 */
template<typename T>
class EmiterSubscription : private Subscription, private Subscriber<T> {
  constexpr static auto kCanceled = credits::kCanceled;
  constexpr static auto kNoFlowControl = credits::kNoFlowControl;

 public:
  EmiterSubscription(
      Reference <EmiterBase<T>> emiter,
      Reference <Subscriber<T>> subscriber)
      : emiter_(std::move(emiter)), subscriber_(std::move(subscriber)) {
    // We expect to be heap-allocated; until this subscription finishes
    // (is canceled; completes; error's out), hold a reference so we are
    // not deallocated (by the subscriber).
    Refcounted::incRef(*this);
    subscriber_->onSubscribe(get_ref<Subscription>(this));
  }

  virtual ~EmiterSubscription() {
    subscriber_.reset();
  }

  void request(int64_t delta) override {
    if (delta <= 0) {
      auto message = "request(n): " + std::to_string(delta) + " <= 0";
      throw std::logic_error(message);
    }

    while (true) {
      auto current = requested_.load(std::memory_order_relaxed);

      if (current == kCanceled) {
        // this can happen because there could be an async barrier between
        // the subscriber and the subscription
        // for instance while onComplete is being delivered
        // (on effectively cancelled subscription) the subscriber can call call request(n)
        return;
      }

      auto const total = credits::add(current, delta);
      if (requested_.compare_exchange_strong(current, total)) {
        break;
      }
    }

    process();
  }

  void cancel() override {
    // if this is the first terminating signal to receive, we need to
    // make sure we break the reference cycle between subscription and
    // subscriber
    //
    auto previous = requested_.exchange(kCanceled, std::memory_order_relaxed);
    if (previous != kCanceled) {
      // this can happen because there could be an async barrier between
      // the subscriber and the subscription
      // for instance while onComplete is being delivered
      // (on effectively cancelled subscription) the subscriber can call call request(n)
      process();
    }
  }

  // Subscriber methods.
  void onSubscribe(Reference<Subscription>) override {
    LOG(FATAL) << "Do not call this method";
  }

  void onNext(T value) override {
    subscriber_->onNext(std::move(value));
  }

  void onComplete() override {
    // we will set the flag first to save a potential call to lock.try_lock()
    // in the process method via cancel or request methods
    auto old = requested_.exchange(kCanceled, std::memory_order_relaxed);
    DCHECK_NE(old, kCanceled) << "Calling onComplete or onError twice or on "
                              << "canceled subscription";

    subscriber_->onComplete();
    // We should already be in process(); nothing more to do.
    //
    // Note: we're not invoking the Subscriber superclass' method:
    // we're following the Subscription's protocol instead.
  }

  void onError(folly::exception_wrapper error) override {
    // we will set the flag first to save a potential call to lock.try_lock()
    // in the process method via cancel or request methods
    auto old = requested_.exchange(kCanceled, std::memory_order_relaxed);
    DCHECK_NE(old, kCanceled) << "Calling onComplete or onError twice or on "
                              << "canceled subscription";

    subscriber_->onError(error);
    // We should already be in process(); nothing more to do.
    //
    // Note: we're not invoking the Subscriber superclass' method:
    // we're following the Subscription's protocol instead.
  }

 private:
  // Processing loop.  Note: this can delete `this` upon completion,
  // error, or cancellation; thus, no fields should be accessed once
  // this method returns.
  //
  // Thread-Safety: there is no guarantee as to which thread this is
  // invoked on.  However, there is a strong guarantee on cancel and
  // request(n) calls: no more than one instance of either of these
  // can be outstanding at any time.
  void process() {
    // This lock guards against re-entrancy in request(n) calls.  By
    // the strict terms of the subscriber guarantees, this could be
    // replaced by a re-entrancy count.
    std::unique_lock<std::mutex> lock(processing_, std::defer_lock);
    if (!lock.try_lock()) {
      return;
    }

    while (true) {
      auto current = requested_.load(std::memory_order_relaxed);

      // Subscription was canceled, completed, or had an error.
      if (current == kCanceled) {
        // Don't destroy a locked mutex.
        lock.unlock();

        release();
        return;
      }

      // If no more items can be emitted now, wait for a request(n).
      // See note above re: thread-safety.  We are guaranteed that
      // request(n) is not simultaneously invoked on another thread.
      if (current <= 0)
        return;

      int64_t emitted;
      bool done;

      std::tie(emitted, done) =
          emiter_->emit(get_ref<Subscriber<T>>(this), current);

      while (true) {
        current = requested_.load(std::memory_order_relaxed);
        if (current == kCanceled || (current == kNoFlowControl && !done)) {
          break;
        }

        auto updated = done ? kCanceled : current - emitted;
        if (requested_.compare_exchange_strong(current, updated)) {
          break;
        }
      }
    }
  }

  void release() {
    emiter_.reset();
    subscriber_.reset();
    Refcounted::decRef(*this);
  }

  // The number of items that can be sent downstream.  Each request(n)
  // adds n; each onNext consumes 1.  If this is MAX, flow-control is
  // disabled: items sent downstream don't consume any longer.  A MIN
  // value represents cancellation.  Other -ve values aren't permitted.
  std::atomic_int_fast64_t requested_{0};

  // We don't want to recursively invoke process(); one loop should do.
  std::mutex processing_;

  Reference <EmiterBase<T>> emiter_;
  Reference <Subscriber<T>> subscriber_;
};


template<typename T, typename Emitter>
class EmitterWrapper : public EmiterBase<T>, public Flowable<T> {
 public:
  explicit EmitterWrapper(Emitter emitter)
      : emitter_(std::move(emitter)) {}

  void subscribe(Reference <Subscriber<T>> subscriber) override {
    new EmiterSubscription<T>(get_ref(this), std::move(subscriber));
  }

  std::tuple<int64_t, bool> emit(
      Reference <Subscriber<T>> subscriber,
      int64_t requested) override {
    return emitter_(std::move(subscriber), requested);
  }

 private:
  Emitter emitter_;
};

} // details
} // flowable
} // yarpl
