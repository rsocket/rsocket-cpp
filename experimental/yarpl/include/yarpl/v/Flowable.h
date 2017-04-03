#pragma once

#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

#include <boost/intrusive_ptr.hpp>
#include <boost/smart_ptr/intrusive_ref_counter.hpp>

#include "reactivestreams/ReactiveStreams.h"
#include "yarpl/utils/type_traits.h"

#include "Subscriber.h"

namespace yarpl {

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"

template<typename T>
class Flowable : public boost::intrusive_ref_counter<Flowable<T>> {
public:
  using Handle = boost::intrusive_ptr<Flowable<T>>;
  using Subscriber = Subscriber<T>;

  virtual ~Flowable() = default;
  virtual void subscribe(std::unique_ptr<Subscriber>) = 0;

  template<typename Function>
  auto map(Function&& function);

  /**
   * Create a flowable from an emitter.
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
  template<typename Emitter, typename = typename std::enable_if<
      std::is_callable<Emitter(Subscriber&, int64_t),
                       std::tuple<int64_t, bool>>::value>::type>
  static Handle create(Emitter&& emitter) {
    return Handle(new Wrapper<Emitter>(std::forward<Emitter>(emitter)));
  }

private:
  virtual std::tuple<int64_t, bool> emit(Subscriber&, int64_t) = 0;

  template<typename Emitter>
  class Wrapper : public Flowable {
  public:
    Wrapper(Emitter&& emitter)
      : emitter_(std::forward<Emitter>(emitter)) {}

    virtual void subscribe(std::unique_ptr<Subscriber> subscriber) {
      new SynchronousSubscription(this, std::move(subscriber));
    }

    virtual std::tuple<int64_t, bool> emit(
        Subscriber& subscriber, int64_t requested) {
      return emitter_(subscriber, requested);
    }

  private:
    Emitter emitter_;
  };

  /**
   * Manager for a flowable subscription.
   *
   * This is synchronous: the emit calls are triggered within the context
   * of a request(n) call.
   */
  class SynchronousSubscription : public Subscription, public Subscriber {
  public:
    SynchronousSubscription(
        Flowable::Handle handle, std::unique_ptr<Subscriber> subscriber)
      : flowable_(handle), subscriber_(std::move(subscriber)) {
      subscriber_->onSubscribe(Handle(this));
    }

    virtual void request(int64_t delta) override {
      static auto max = std::numeric_limits<int64_t>::max();
      if (delta <= 0) {
        auto message = "request(n): " + std::to_string(delta) + " <= 0";
        throw std::logic_error(message);
      }

      while (true) {
        auto current = requested_.load(std::memory_order_relaxed);
        auto total = current + delta;
        if (total < current) {
          // overflow; cap at max (turn flow control off).
          total = max;
        }

        if (requested_.compare_exchange_strong(current, total))
          break;
      }

      process();
    }

    virtual void cancel() override {
      static auto min = std::numeric_limits<int64_t>::min();
      auto previous = requested_.exchange(min, std::memory_order_relaxed);
      if (previous != min) {
        // we're first to mark the cancellation.  Cancel the upstream.
      }

      // Note: process() will finalize: releasing our reference on this.
      process();
    }

    // Subscriber methods.
    virtual void onSubscribe(Subscription::Handle) override {
      // Not actually expected to be called.
    }

    virtual void onNext(const T& value) override {
      subscriber_->onNext(value);
    }

    virtual void onComplete() override {
      static auto min = std::numeric_limits<int64_t>::min();
      subscriber_->onComplete();
      requested_.store(min, std::memory_order_relaxed);
      // We should already be in process(); nothing more to do.
      //
      // Note: we're not invoking the Subscriber superclass' method:
      // we're following the Subscription's protocol instead.
    }

    virtual void onError(const std::exception_ptr error) override {
      static auto min = std::numeric_limits<int64_t>::min();
      subscriber_->onError(error);
      requested_.store(min, std::memory_order_relaxed);
      // We should already be in process(); nothing more to do.
      //
      // Note: we're not invoking the Subscriber superclass' method:
      // we're following the Subscription's protocol instead.
    }

  private:
    // Processing loop.  Note: this can delete `this` upon completion,
    // error, or cancellation; thus, no fields should be accessed once
    // this method returns.
    void process() {
      static auto min = std::numeric_limits<int64_t>::min();
      static auto max = std::numeric_limits<int64_t>::max();

      std::unique_lock<std::mutex> lock(processing_, std::defer_lock);
      if (!lock.try_lock()) {
        return;
      }

      while (true) {
        auto current = requested_.load(std::memory_order_relaxed);
        if (current == min) {
          // Subscription was canceled, completed, or had an error.
          return release();
        }

        // If no more items can be emitted now, wait for a request(n).
        if (current <= 0) return;

        int64_t emitted;
        bool done;

        std::tie(emitted, done) = flowable_->emit(
              *this /* implicit conversion to subscriber */, current);

        while (true) {
          auto current = requested_.load(std::memory_order_relaxed);
          if (current == min || (current == max && !done))
            break;

          auto updated = done ? min : current - emitted;
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

    Flowable::Handle flowable_;
    std::unique_ptr<Subscriber> subscriber_;
  };
};

#pragma clang diagnostic pop

}  // yarpl

#include "Operator.h"

namespace yarpl {

template<typename T>
template<typename Function>
auto Flowable<T>::map(Function&& function) {
  using D = typename std::result_of<Function(T)>::type;
  return typename Flowable<D>::Handle(new MapOperator<T, D, Function>(
      Flowable<T>::Handle(this), std::forward<Function>(function)));
}

}  // yarpl
