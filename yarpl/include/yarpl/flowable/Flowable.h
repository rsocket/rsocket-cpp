// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <folly/Executor.h>
#include <folly/functional/Invoke.h>
#include <glog/logging.h>
#include <memory>
#include <stdexcept>
#include <string>
#include "yarpl/Refcounted.h"
#include "yarpl/flowable/Subscriber.h"
#include "yarpl/flowable/Subscribers.h"
#include "yarpl/utils/credits.h"
#include "yarpl/utils/type_traits.h"

namespace yarpl {
namespace flowable {

template <typename T = void>
class Flowable;

namespace details {

template <typename T>
struct IsFlowable : std::false_type {};

template <typename R>
struct IsFlowable<std::shared_ptr<Flowable<R>>> : std::true_type {
  using ElemType = R;
};

template <typename T>
class TrackingSubscriber;

} // namespace details

template <typename T>
class Flowable : public yarpl::enable_get_ref {
 public:
  virtual ~Flowable() = default;

  virtual void subscribe(std::shared_ptr<Subscriber<T>>) = 0;

  /**
   * Subscribe overload that accepts lambdas.
   */
  template <
      typename Next,
      typename =
          typename std::enable_if<folly::is_invocable<Next, T>::value>::type>
  void subscribe(Next next, int64_t batch = credits::kNoFlowControl) {
    subscribe(Subscribers::create<T>(std::move(next), batch));
  }

  /**
   * Subscribe overload that accepts lambdas.
   *
   * Takes an optional batch size for request_n. Default is no flow control.
   */
  template <
      typename Next,
      typename Error,
      typename = typename std::enable_if<
          folly::is_invocable<Next, T>::value &&
          folly::is_invocable<Error, folly::exception_wrapper>::value>::type>
  void
  subscribe(Next next, Error error, int64_t batch = credits::kNoFlowControl) {
    subscribe(Subscribers::create<T>(std::move(next), std::move(error), batch));
  }

  /**
   * Subscribe overload that accepts lambdas.
   *
   * Takes an optional batch size for request_n. Default is no flow control.
   */
  template <
      typename Next,
      typename Error,
      typename Complete,
      typename = typename std::enable_if<
          folly::is_invocable<Next, T>::value &&
          folly::is_invocable<Error, folly::exception_wrapper>::value &&
          folly::is_invocable<Complete>::value>::type>
  void subscribe(
      Next next,
      Error error,
      Complete complete,
      int64_t batch = credits::kNoFlowControl) {
    subscribe(Subscribers::create<T>(
        std::move(next), std::move(error), std::move(complete), batch));
  }

  void subscribe() {
    subscribe(Subscribers::createNull<T>());
  }

  //
  // creator methods:
  //

  static std::shared_ptr<Flowable<T>> empty() {
    auto lambda = [](Subscriber<T>& subscriber, int64_t) {
      subscriber.onComplete();
    };
    return Flowable<T>::create(std::move(lambda));
  }

  static std::shared_ptr<Flowable<T>> never() {
    auto lambda = [](details::TrackingSubscriber<T>& subscriber, int64_t) {
      subscriber.setCompleted();
    };
    return Flowable<T>::create(std::move(lambda));
  }

  static std::shared_ptr<Flowable<T>> error(folly::exception_wrapper ex) {
    auto lambda = [ex = std::move(ex)](Subscriber<T>& subscriber, int64_t) {
      subscriber.onError(std::move(ex));
    };
    return Flowable<T>::create(std::move(lambda));
  }

  template <typename Ex>
  static std::shared_ptr<Flowable<T>> error(Ex&) {
    static_assert(
        std::is_lvalue_reference<Ex>::value,
        "use variant of error() method accepting also exception_ptr");
  }

  template <typename Ex>
  static std::shared_ptr<Flowable<T>> error(Ex& ex, std::exception_ptr ptr) {
    auto lambda = [ew = folly::exception_wrapper(std::move(ptr), ex)](
                      Subscriber<T>& subscriber, int64_t) {
      subscriber.onError(std::move(ew));
    };
    return Flowable<T>::create(std::move(lambda));
  }

  template <
      typename OnSubscribe,
      typename = typename std::enable_if<folly::is_invocable<
          OnSubscribe,
          std::shared_ptr<Subscriber<T>>>::value>::type>
  static std::shared_ptr<Flowable<T>> fromPublisher(OnSubscribe function);

  template <typename TGenerator>
  static std::shared_ptr<Flowable<T>> fromGenerator(TGenerator generator);

  /**
   * The Defer operator waits until a subscriber subscribes to it, and then it
   * generates a Flowabe with a FlowableFactory function. It
   * does this afresh for each subscriber, so although each subscriber may
   * think it is subscribing to the same Flowable, in fact each subscriber
   * gets its own individual sequence.
   */
  template <
      typename FlowableFactory,
      typename = typename std::enable_if<folly::is_invocable_r<
          std::shared_ptr<Flowable<T>>,
          FlowableFactory>::value>::type>
  static std::shared_ptr<Flowable<T>> defer(FlowableFactory);

  template <
      typename Function,
      typename R = typename std::result_of<Function(T)>::type>
  std::shared_ptr<Flowable<R>> map(Function function);

  template <
      typename Function,
      typename R = typename details::IsFlowable<
          typename std::result_of<Function(T)>::type>::ElemType>
  std::shared_ptr<Flowable<R>> flatMap(Function func);

  template <typename Function>
  std::shared_ptr<Flowable<T>> filter(Function function);

  template <
      typename Function,
      typename R = typename std::result_of<Function(T, T)>::type>
  std::shared_ptr<Flowable<R>> reduce(Function function);

  std::shared_ptr<Flowable<T>> take(int64_t);

  std::shared_ptr<Flowable<T>> skip(int64_t);

  std::shared_ptr<Flowable<T>> ignoreElements();

  /*
   * To instruct a Flowable to do its work on a particular Executor.
   */
  std::shared_ptr<Flowable<T>> subscribeOn(folly::Executor&);

  std::shared_ptr<Flowable<T>> observeOn(folly::Executor&);

  template <typename Q>
  using enableWrapRef =
      typename std::enable_if<details::IsFlowable<Q>::value, Q>::type;

  template <typename Q = T>
  enableWrapRef<Q> merge() {
    return this->flatMap([](auto f) { return std::move(f); });
  }

  // function is invoked when onComplete occurs.
  template <
      typename Function,
      typename =
          typename std::enable_if<folly::is_invocable<Function>::value>::type>
  std::shared_ptr<Flowable<T>> doOnSubscribe(Function function);

  // function is invoked when onNext occurs.
  template <
      typename Function,
      typename = typename std::enable_if<
          folly::is_invocable<Function, const T&>::value>::type>
  std::shared_ptr<Flowable<T>> doOnNext(Function function);

  // function is invoked when onError occurs.
  template <
      typename Function,
      typename = typename std::enable_if<
          folly::is_invocable<Function, folly::exception_wrapper&>::value>::
          type>
  std::shared_ptr<Flowable<T>> doOnError(Function function);

  // function is invoked when onComplete occurs.
  template <
      typename Function,
      typename =
          typename std::enable_if<folly::is_invocable<Function>::value>::type>
  std::shared_ptr<Flowable<T>> doOnComplete(Function function);

  // function is invoked when either onComplete or onError occurs.
  template <
      typename Function,
      typename =
          typename std::enable_if<folly::is_invocable<Function>::value>::type>
  std::shared_ptr<Flowable<T>> doOnTerminate(Function function);

  // the function is invoked for each of onNext, onCompleted, onError
  template <
      typename Function,
      typename =
          typename std::enable_if<folly::is_invocable<Function>::value>::type>
  std::shared_ptr<Flowable<T>> doOnEach(Function function);

  // function is invoked when request(n) is called.
  template <
      typename Function,
      typename = typename std::enable_if<
          folly::is_invocable<Function, int64_t>::value>::type>
  std::shared_ptr<Flowable<T>> doOnRequest(Function function);

  // function is invoked when cancel is called.
  template <
      typename Function,
      typename =
          typename std::enable_if<folly::is_invocable<Function>::value>::type>
  std::shared_ptr<Flowable<T>> doOnCancel(Function function);

  // the callbacks will be invoked of each of the signals
  template <
      typename OnNextFunc,
      typename OnCompleteFunc,
      typename = typename std::enable_if<
          folly::is_invocable<OnNextFunc, const T&>::value>::type,
      typename = typename std::enable_if<
          folly::is_invocable<OnCompleteFunc>::value>::type>
  std::shared_ptr<Flowable<T>> doOn(
      OnNextFunc onNext,
      OnCompleteFunc onComplete);

  // the callbacks will be invoked of each of the signals
  template <
      typename OnNextFunc,
      typename OnCompleteFunc,
      typename OnErrorFunc,
      typename = typename std::enable_if<
          folly::is_invocable<OnNextFunc, const T&>::value>::type,
      typename = typename std::enable_if<
          folly::is_invocable<OnCompleteFunc>::value>::type,
      typename = typename std::enable_if<
          folly::is_invocable<OnErrorFunc, folly::exception_wrapper&>::value>::
          type>
  std::shared_ptr<Flowable<T>>
  doOn(OnNextFunc onNext, OnCompleteFunc onComplete, OnErrorFunc onError);

  template <
      typename Emitter,
      typename = typename std::enable_if<folly::is_invocable_r<
          void,
          Emitter,
          details::TrackingSubscriber<T>&,
          int64_t>::value>::type>
  static std::shared_ptr<Flowable<T>> create(Emitter emitter);
};

} // namespace flowable
} // namespace yarpl

#include "yarpl/flowable/DeferFlowable.h"
#include "yarpl/flowable/EmitterFlowable.h"
#include "yarpl/flowable/FlowableOperator.h"

namespace yarpl {
namespace flowable {

template <typename T>
template <typename Emitter, typename>
std::shared_ptr<Flowable<T>> Flowable<T>::create(Emitter emitter) {
  return std::make_shared<details::EmitterWrapper<T, Emitter>>(
      std::move(emitter));
}

template <typename T>
template <typename OnSubscribe, typename>
std::shared_ptr<Flowable<T>> Flowable<T>::fromPublisher(OnSubscribe function) {
  return std::make_shared<FromPublisherOperator<T, OnSubscribe>>(
      std::move(function));
}

template <typename T>
template <typename TGenerator>
std::shared_ptr<Flowable<T>> Flowable<T>::fromGenerator(TGenerator generator) {
  auto lambda = [generator = std::move(generator)](
                    Subscriber<T>& subscriber, int64_t requested) {
    try {
      while (requested-- > 0) {
        subscriber.onNext(generator());
      }
    } catch (const std::exception& ex) {
      subscriber.onError(
          folly::exception_wrapper(std::current_exception(), ex));
    } catch (...) {
      subscriber.onError(std::runtime_error("unknown error"));
    }
  };
  return Flowable<T>::create(std::move(lambda));
}

template <typename T>
template <typename FlowableFactory, typename>
std::shared_ptr<Flowable<T>> Flowable<T>::defer(FlowableFactory factory) {
  return std::make_shared<details::DeferFlowable<T, FlowableFactory>>(
      std::move(factory));
}

template <typename T>
template <typename Function, typename R>
std::shared_ptr<Flowable<R>> Flowable<T>::map(Function function) {
  return std::make_shared<MapOperator<T, R, Function>>(
      this->ref_from_this(this), std::move(function));
}

template <typename T>
template <typename Function>
std::shared_ptr<Flowable<T>> Flowable<T>::filter(Function function) {
  return std::make_shared<FilterOperator<T, Function>>(
      this->ref_from_this(this), std::move(function));
}

template <typename T>
template <typename Function, typename R>
std::shared_ptr<Flowable<R>> Flowable<T>::reduce(Function function) {
  return std::make_shared<ReduceOperator<T, R, Function>>(
      this->ref_from_this(this), std::move(function));
}

template <typename T>
std::shared_ptr<Flowable<T>> Flowable<T>::take(int64_t limit) {
  return std::make_shared<TakeOperator<T>>(this->ref_from_this(this), limit);
}

template <typename T>
std::shared_ptr<Flowable<T>> Flowable<T>::skip(int64_t offset) {
  return std::make_shared<SkipOperator<T>>(this->ref_from_this(this), offset);
}

template <typename T>
std::shared_ptr<Flowable<T>> Flowable<T>::ignoreElements() {
  return std::make_shared<IgnoreElementsOperator<T>>(this->ref_from_this(this));
}

template <typename T>
std::shared_ptr<Flowable<T>> Flowable<T>::subscribeOn(
    folly::Executor& executor) {
  return std::make_shared<SubscribeOnOperator<T>>(
      this->ref_from_this(this), executor);
}

template <typename T>
std::shared_ptr<Flowable<T>> Flowable<T>::observeOn(folly::Executor& executor) {
  return std::make_shared<yarpl::flowable::detail::ObserveOnOperator<T>>(
      this->ref_from_this(this), executor);
}

template <typename T>
template <typename Function, typename R>
std::shared_ptr<Flowable<R>> Flowable<T>::flatMap(Function function) {
  return std::make_shared<FlatMapOperator<T, R>>(
      this->ref_from_this(this), std::move(function));
}

template <typename T>
template <typename Function, typename>
std::shared_ptr<Flowable<T>> Flowable<T>::doOnSubscribe(Function function) {
  return details::createDoOperator(
      ref_from_this(this),
      std::move(function),
      [](const T&) {},
      [](const auto&) {},
      [] {},
      [](const auto&) {}, // onRequest
      [] {}); // onCancel
}

template <typename T>
template <typename Function, typename>
std::shared_ptr<Flowable<T>> Flowable<T>::doOnNext(Function function) {
  return details::createDoOperator(
      ref_from_this(this),
      [] {},
      std::move(function),
      [](const auto&) {},
      [] {},
      [](const auto&) {}, // onRequest
      [] {}); // onCancel
}

template <typename T>
template <typename Function, typename>
std::shared_ptr<Flowable<T>> Flowable<T>::doOnError(Function function) {
  return details::createDoOperator(
      ref_from_this(this),
      [] {},
      [](const T&) {},
      std::move(function),
      [] {},
      [](const auto&) {}, // onRequest
      [] {}); // onCancel
}

template <typename T>
template <typename Function, typename>
std::shared_ptr<Flowable<T>> Flowable<T>::doOnComplete(Function function) {
  return details::createDoOperator(
      ref_from_this(this),
      [] {},
      [](const T&) {},
      [](const auto&) {},
      std::move(function),
      [](const auto&) {}, // onRequest
      [] {}); // onCancel
}

template <typename T>
template <typename Function, typename>
std::shared_ptr<Flowable<T>> Flowable<T>::doOnTerminate(Function function) {
  auto sharedFunction = std::make_shared<Function>(std::move(function));
  return details::createDoOperator(
      ref_from_this(this),
      [] {},
      [](const T&) {},
      [sharedFunction](const auto&) { (*sharedFunction)(); },
      [sharedFunction]() { (*sharedFunction)(); },
      [](const auto&) {}, // onRequest
      [] {}); // onCancel
}

template <typename T>
template <typename Function, typename>
std::shared_ptr<Flowable<T>> Flowable<T>::doOnEach(Function function) {
  auto sharedFunction = std::make_shared<Function>(std::move(function));
  return details::createDoOperator(
      ref_from_this(this),
      [] {},
      [sharedFunction](const T&) { (*sharedFunction)(); },
      [sharedFunction](const auto&) { (*sharedFunction)(); },
      [sharedFunction]() { (*sharedFunction)(); },
      [](const auto&) {}, // onRequest
      [] {}); // onCancel
}

template <typename T>
template <typename OnNextFunc, typename OnCompleteFunc, typename, typename>
std::shared_ptr<Flowable<T>> Flowable<T>::doOn(
    OnNextFunc onNext,
    OnCompleteFunc onComplete) {
  return details::createDoOperator(
      ref_from_this(this),
      [] {},
      std::move(onNext),
      [](const auto&) {},
      std::move(onComplete),
      [](const auto&) {}, // onRequest
      [] {}); // onCancel
}

template <typename T>
template <
    typename OnNextFunc,
    typename OnCompleteFunc,
    typename OnErrorFunc,
    typename,
    typename,
    typename>
std::shared_ptr<Flowable<T>> Flowable<T>::doOn(
    OnNextFunc onNext,
    OnCompleteFunc onComplete,
    OnErrorFunc onError) {
  return details::createDoOperator(
      ref_from_this(this),
      [] {},
      std::move(onNext),
      std::move(onError),
      std::move(onComplete),
      [](const auto&) {}, // onRequest
      [] {}); // onCancel
}

template <typename T>
template <typename Function, typename>
std::shared_ptr<Flowable<T>> Flowable<T>::doOnRequest(Function function) {
  return details::createDoOperator(
      ref_from_this(this),
      [] {}, // onSubscribe
      [](const auto&) {}, // onNext
      [](const auto&) {}, // onError
      [] {}, // onComplete
      std::move(function), // onRequest
      [] {}); // onCancel
}

template <typename T>
template <typename Function, typename>
std::shared_ptr<Flowable<T>> Flowable<T>::doOnCancel(Function function) {
  return details::createDoOperator(
      ref_from_this(this),
      [] {}, // onSubscribe
      [](const auto&) {}, // onNext
      [](const auto&) {}, // onError
      [] {}, // onComplete
      [](const auto&) {}, // onRequest
      std::move(function)); // onCancel
}

} // namespace flowable
} // namespace yarpl
