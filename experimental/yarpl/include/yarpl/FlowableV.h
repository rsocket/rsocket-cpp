// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <atomic>
#include <initializer_list>
#include <iostream>
#include <limits>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>

#include <boost/intrusive_ptr.hpp>
#include <boost/smart_ptr/intrusive_ref_counter.hpp>

#include "reactivestreams/ReactiveStreams.h"
#include "yarpl/utils/type_traits.h"

namespace yarpl {

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"

class Subscription : public reactivestreams_yarpl::Subscription,
    public boost::intrusive_ref_counter<Subscription> {
public:
  using Handle = boost::intrusive_ptr<Subscription>;
  virtual ~Subscription() = default;

  virtual void request(int64_t n) = 0;
  virtual void cancel() = 0;

protected:
  Subscription() : reference_(this) {}

  // Drop the reference we're holding on the subscription (handle).
  void release() {
    reference_.reset();
  }

private:
  // We expect to be heap-allocated; until this subscription finishes
  // (is canceled; completes; error's out), hold a reference so we are
  // not deallocated (by the subscriber).
  Handle reference_{this};
};

template<typename T>
class Subscriber : public reactivestreams_yarpl::Subscriber<T> {
protected:
  static const auto max = std::numeric_limits<int64_t>::max();

public:
  // Note: if any of the following methods is overridden in a subclass,
  // the new methods SHOULD ensure that these are invoked as well.
  virtual void onSubscribe(Subscription::Handle subscription) {
    subscription_ = subscription;
  }

  // No further calls to the subscription after this method is invoked.
  virtual void onComplete() {
    subscription_.reset();
  }

  // No further calls to the subscription after this method is invoked.
  virtual void onError(const std::exception_ptr) {
    subscription_.reset();
  }

  virtual void onNext(const T&) {}
  virtual void onNext(T&& value) { onNext(value); }

protected:
  Subscription* subscription() {
    return subscription_.operator->();
  }

private:
  // "Our" reference to the subscription, to ensure that it is retained
  // while calls to its methods are in-flight.
  Subscription::Handle subscription_;

  // Note: we've overridden the signature of onSubscribe with yarpl's
  // Subscriber.  Keep this definition, making it private, to keep the
  // C++ compilers from issuing a warning about the override.
  virtual void onSubscribe(reactivestreams_yarpl::Subscription*) {}
};

template<typename T>
class Flowable : public boost::intrusive_ref_counter<Flowable<T>> {

public:
  using Handle = boost::intrusive_ptr<Flowable<T>>;
  using Subscriber = Subscriber<T>;

  virtual ~Flowable() = default;
  virtual void subscribe(std::unique_ptr<Subscriber>) = 0;

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

    virtual void onNext(T&& value) override {
      subscriber_->onNext(std::move(value));
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

  template<typename Downstream>
  class Operator : public Flowable, public Subscriber {
  public:
    Operator(Flowable::Handle upstream) : upstream_(upstream) {}

    virtual void subscribe(std::unique_ptr<Subscriber> subscriber) override {
      upstream_->subscribe(this);
    }
  private:
    Flowable::Handle upstream_;
  };
};

#pragma clang diagnostic pop

class Flowables {
public:
  static Flowable<int64_t>::Handle range(int64_t start, int64_t end) {
    auto lambda = [start, end, i = start](
        Subscriber<int64_t>& subscriber, int64_t requested) mutable {
      int64_t emitted = 0;
      bool done = false;

      while (i < end && emitted < requested) {
        subscriber.onNext(i++);
        ++emitted;
      }

      if (i >= end) {
        subscriber.onComplete();
        done = true;
      }

      return std::make_tuple(requested, done);
    };

    return Flowable<int64_t>::create(std::move(lambda));
  }

  template<typename T>
  static typename Flowable<T>::Handle just(const T& value) {
    auto lambda = [value](Subscriber<T>& subscriber, int64_t) {
      // # requested should be > 0.  Ignoring the actual parameter.
      subscriber.onNext(value);
      subscriber.onComplete();
      return std::make_tuple(static_cast<int64_t>(1), true);
    };

    return Flowable<T>::create(std::move(lambda));
  }

  template<typename T>
  static typename Flowable<T>::Handle just(std::initializer_list<T> list) {
    auto lambda = [list, it = list.begin()](
        Subscriber<T>& subscriber, int64_t requested) mutable {
      int64_t emitted = 0;
      bool done = false;

      while (it != list.end() && emitted < requested) {
        subscriber.onNext(*it++);
        ++emitted;
      }

      if (it == list.end()) {
        subscriber.onComplete();
        done = true;
      }

      return std::make_tuple(static_cast<int64_t>(emitted), done);
    };

    return Flowable<T>::create(std::move(lambda));
  }

private:
  Flowables() = delete;
};

class Subscribers {
  static const auto max = std::numeric_limits<int64_t>::max();

public:
  template<typename T, typename N>
  static auto create(N&& next, int64_t batch = max) {
    class Derived : public Subscriber<T> {
    public:
      Derived(N&& next, int64_t batch)
        : next_(std::forward<N>(next)), batch_(batch), pending_(0) {}

      virtual void onSubscribe(Subscription::Handle subscription) override {
        Subscriber<T>::onSubscribe(subscription);
        pending_ += batch_;
        subscription->request(batch_);
      }

      virtual void onNext(const T& value) override {
        next_(value);
        if (--pending_ < batch_/2) {
          const auto delta = batch_ - pending_;
          pending_ += delta;
          Subscriber<T>::subscription()->request(delta);
        }
      }

    private:
      N next_;
      const int64_t batch_;
      int64_t pending_;
    };

    return std::make_unique<Derived>(std::forward<N>(next), batch);
  }

private:
  Subscribers() = delete;
};

}  // yarpl


//  /**
//   * Lift an operator into F<T> and return F<R>
//   * @tparam R
//   * @tparam F
//   * @param onSubscribeLift
//   * @return
//   */
//  template <
//      typename R,
//      typename F,
//      typename = typename std::enable_if<std::is_callable<
//          F(std::unique_ptr<reactivestreams_yarpl::Subscriber<R>>),
//          std::unique_ptr<reactivestreams_yarpl::Subscriber<T>>>::value>::type>
//  std::unique_ptr<FlowableV<R>> lift(F&& onSubscribeLift) {
//    return FlowableV<R>::create(
//        [ this, onSub = std::move(onSubscribeLift) ](auto sOfR) mutable {
//          this->subscribe(std::move(onSub(std::move(sOfR))));
//        });
//  }

//  /**
//   * Map F<T> -> F<R>
//   *
//   * @tparam F
//   * @param function
//   * @return
//   */
//  template <
//      typename F,
//      typename = typename std::enable_if<
//          std::is_callable<F(T), typename std::result_of<F(T)>::type>::value>::
//          type>
//  std::unique_ptr<FlowableV<typename std::result_of<F(T)>::type>> map(
//      F&& function);

//  /**
//   * Take n items from F<T> then cancel.
//   * @param toTake
//   * @return
//   */
//  std::unique_ptr<FlowableV<T>> take(int64_t toTake) {
//    return lift<T>(yarpl::operators::FlowableTakeOperator<T>(toTake));
//  }

//  /**
//   * SubscribeOn the given Scheduler
//   * @param scheduler
//   * @return
//   */
//  std::unique_ptr<FlowableV<T>> subscribeOn(yarpl::Scheduler& scheduler) {
//    return lift<T>(yarpl::operators::FlowableSubscribeOnOperator<T>(scheduler));
//  }

// protected:
//  FlowableV() = default;


//template <typename T>
//template <typename F, typename Default>
//std::unique_ptr<FlowableV<typename std::result_of<F(T)>::type>>
//FlowableV<T>::map(F&& function) {
//  return lift<typename std::result_of<F(T)>::type>(
//      yarpl::operators::
//          FlowableMapOperator<T, typename std::result_of<F(T)>::type, F>(
//              std::forward<F>(function)));
//}

