#pragma once

#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

#include "yarpl/Scheduler.h"
#include "yarpl/utils/type_traits.h"

#include "Refcounted.h"
#include "flowable/Subscriber.h"

namespace yarpl {
namespace flowable {

template <typename T>
class Flowable : public virtual Refcounted {
 public:
  static const auto CANCELED = std::numeric_limits<int64_t>::min();
  static const auto NO_FLOW_CONTROL = std::numeric_limits<int64_t>::max();

  virtual void subscribe(Reference<Subscriber<T>>) = 0;

  template <typename Function>
  auto map(Function&& function);

  auto take(int64_t);

  auto subscribeOn(Scheduler&);

  /**
   * \brief Create a flowable from an emitter.
   *
   * \param emitter function that is invoked to emit values to a subscriber.
   * The emitter's signature is:
   * \code{.cpp}
   *     std::tuple<int64_t, bool> emitter(Subscriber&, int64_t requested);
   * \endcode
   *
   * The emitter can invoke up to \b requested calls to `onNext()`, and can
   * optionally make a final call to `onComplete()` or `onError()`; returns
   * the actual number of `onNext()` calls; and whether the subscription is
   * finished (completed/in error).
   *
   * \return a handle to a flowable that will use the emitter.
   */
  template <typename Emitter>
  class EmitterWrapper;

  template <
      typename Emitter,
      typename = typename std::enable_if<std::is_callable<
          Emitter(Subscriber<T>&, int64_t),
          std::tuple<int64_t, bool>>::value>::type>
  static auto create(Emitter&& emitter);

 private:
  virtual std::tuple<int64_t, bool> emit(Subscriber<T>&, int64_t) {
    return std::make_tuple(static_cast<int64_t>(0), false);
  }

  /**
   * Manager for a flowable subscription.
   *
   * This is synchronous: the emit calls are triggered within the context
   * of a request(n) call.
   */
  class SynchronousSubscription : public Subscription, public Subscriber<T> {
   public:
    SynchronousSubscription(
        Reference<Flowable> flowable,
        Reference<Subscriber<T>> subscriber)
        : flowable_(std::move(flowable)), subscriber_(std::move(subscriber)) {
      subscriber_->onSubscribe(Reference<Subscription>(this));
    }

    virtual ~SynchronousSubscription() {
      subscriber_.reset();
    }

    virtual void request(int64_t delta) override {
      if (delta <= 0) {
        auto message = "request(n): " + std::to_string(delta) + " <= 0";
        throw std::logic_error(message);
      }

      while (true) {
        auto current = requested_.load(std::memory_order_relaxed);

        // Turn flow control off for overflow.
        auto const total =
          (current > std::numeric_limits<int64_t>::max() - delta)
          ? NO_FLOW_CONTROL
          : current + delta;

        if (requested_.compare_exchange_strong(current, total))
          break;
      }

      process();
    }

    virtual void cancel() override {
      requested_.exchange(CANCELED, std::memory_order_relaxed);
      process();
    }

    // Subscriber methods.
    virtual void onSubscribe(Reference<Subscription>) override {
      // Not actually expected to be called.
    }

    virtual void onNext(const T& value) override {
      subscriber_->onNext(value);
    }

    virtual void onComplete() override {
      subscriber_->onComplete();
      requested_.store(CANCELED, std::memory_order_relaxed);
      // We should already be in process(); nothing more to do.
      //
      // Note: we're not invoking the Subscriber superclass' method:
      // we're following the Subscription's protocol instead.
    }

    virtual void onError(const std::exception_ptr error) override {
      subscriber_->onError(error);
      requested_.store(CANCELED, std::memory_order_relaxed);
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
        if (current == CANCELED) {
          // Subscription was canceled, completed, or had an error.
          return release();
        }

        // If no more items can be emitted now, wait for a request(n).
        // See note above re: thread-safety.  We are guaranteed that
        // request(n) is not simultaneously invoked on another thread.
        if (current <= 0)
          return;

        int64_t emitted;
        bool done;

        std::tie(emitted, done) = flowable_->emit(
            *this /* implicit conversion to subscriber */, current);

        while (true) {
          auto current = requested_.load(std::memory_order_relaxed);
          if (current == CANCELED || (current == NO_FLOW_CONTROL && !done))
            break;

          auto updated = done ? CANCELED : current - emitted;
          if (requested_.compare_exchange_strong(current, updated))
            break;
        }
      }
    }

    // The number of items that can be sent downstream.  Each request(n)
    // adds n; each onNext consumes 1.  If this is MAX, flow-control is
    // disabled: items sent downstream don't consume any longer.  A MIN
    // value represents cancellation.  Other -ve values aren't permitted.
    std::atomic_int_fast64_t requested_{0};

    // We don't want to recursively invoke process(); one loop should do.
    std::mutex processing_;

    Reference<Flowable> flowable_;
    Reference<Subscriber<T>> subscriber_;
  };
};

} // flowable
} // yarpl

#include "flowable/FlowableOperator.h"

namespace yarpl {
namespace flowable {

template <typename T>
template <typename Emitter>
class Flowable<T>::EmitterWrapper : public Flowable<T> {
 public:
  explicit EmitterWrapper(Emitter&& emitter)
      : emitter_(std::forward<Emitter>(emitter)) {}

  virtual void subscribe(Reference<Subscriber<T>> subscriber) {
    new SynchronousSubscription(
        Reference<Flowable>(this), std::move(subscriber));
  }

  virtual std::tuple<int64_t, bool> emit(
      Subscriber<T>& subscriber,
      int64_t requested) {
    return emitter_(subscriber, requested);
  }

 private:
  Emitter emitter_;
};

template <typename T>
template <typename Emitter, typename>
auto Flowable<T>::create(Emitter&& emitter) {
  return Reference<Flowable<T>>(
      new Flowable<T>::EmitterWrapper<Emitter>(std::forward<Emitter>(emitter)));
}

template <typename T>
template <typename Function>
auto Flowable<T>::map(Function&& function) {
  using D = typename std::result_of<Function(T)>::type;
  return Reference<Flowable<D>>(new MapOperator<T, D, Function>(
      Reference<Flowable<T>>(this), std::forward<Function>(function)));
}

template <typename T>
auto Flowable<T>::take(int64_t limit) {
  return Reference<Flowable<T>>(
      new TakeOperator<T>(Reference<Flowable<T>>(this), limit));
}

template <typename T>
auto Flowable<T>::subscribeOn(Scheduler& scheduler) {
  return Reference<Flowable<T>>(
      new SubscribeOnOperator<T>(Reference<Flowable<T>>(this), scheduler));
}

} // flowable
} // yarpl
