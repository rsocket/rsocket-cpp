// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <algorithm>
#include <limits>
#include <vector>

#include <glog/logging.h>

#include "yarpl/flowable/Flowable.h"

namespace yarpl {
namespace flowable {

class Flowables {
 public:
  /**
   * Emit the sequence of numbers [start, start + count).
   */
  static Reference<Flowable<int64_t>> range(int64_t start, int64_t count) {
    auto lambda = [ start, count, i = start ](
        Reference<Subscriber<int64_t>> subscriber, int64_t requested) mutable {
      int64_t emitted = 0;
      bool done = false;
      int64_t end = start + count;

      while (i < end && emitted < requested) {
        subscriber->onNext(i++);
        ++emitted;
      }

      if (i >= end) {
        subscriber->onComplete();
        done = true;
      }

      return std::make_tuple(requested, done);
    };

    return Flowable<int64_t>::create(std::move(lambda));
  }

  template <typename T>
  static Reference<Flowable<T>> just(const T& value) {
    auto lambda = [value](Reference<Subscriber<T>> subscriber, int64_t) {
      // # requested should be > 0.  Ignoring the actual parameter.
      subscriber->onNext(value);
      subscriber->onComplete();
      return std::make_tuple(static_cast<int64_t>(1), true);
    };

    return Flowable<T>::create(std::move(lambda));
  }

  template <typename T>
  static Reference<Flowable<T>> justN(std::initializer_list<T> list) {
    std::vector<T> vec(list);

    auto lambda = [ v = std::move(vec), i = size_t{0} ](
        Reference<Subscriber<T>> subscriber, int64_t requested) mutable {
      int64_t emitted = 0;
      bool done = false;

      while (i < v.size() && emitted < requested) {
        subscriber->onNext(v[i++]);
        ++emitted;
      }

      if (i == v.size()) {
        subscriber->onComplete();
        done = true;
      }

      return std::make_tuple(emitted, done);
    };

    return Flowable<T>::create(std::move(lambda));
  }

  // this will generate a flowable which can be subscribed to only once
  template <typename T>
  static Reference<Flowable<T>> justOnce(T value) {
    auto lambda = [ value = std::move(value), used = false ](
        Reference<Subscriber<T>> subscriber, int64_t) mutable {
      if (used) {
        subscriber->onError(
            std::runtime_error("justOnce value was already used"));
        return std::make_tuple(static_cast<int64_t>(0), true);
      }

      used = true;
      // # requested should be > 0.  Ignoring the actual parameter.
      subscriber->onNext(std::move(value));
      subscriber->onComplete();
      return std::make_tuple(static_cast<int64_t>(1), true);
    };

    return Flowable<T>::create(std::move(lambda));
  }

  template <
      typename T,
      typename OnSubscribe,
      typename = typename std::enable_if<std::is_callable<
          OnSubscribe(Reference<Subscriber<T>>),
          void>::value>::type>
  static Reference<Flowable<T>> fromPublisher(OnSubscribe function) {
    return Reference<Flowable<T>>(
        new FromPublisherOperator<T, OnSubscribe>(std::move(function)));
  }

  template <typename T>
  static Reference<Flowable<T>> empty() {
    auto lambda = [](Reference<Subscriber<T>> subscriber, int64_t) {
      subscriber->onComplete();
      return std::make_tuple(static_cast<int64_t>(0), true);
    };
    return Flowable<T>::create(std::move(lambda));
  }

  template <typename T>
  static Reference<Flowable<T>> error(folly::exception_wrapper ex) {
    auto lambda = [ex = std::move(ex)](
        Reference<Subscriber<T>> subscriber, int64_t) {
      subscriber->onError(std::move(ex));
      return std::make_tuple(static_cast<int64_t>(0), true);
    };
    return Flowable<T>::create(std::move(lambda));
  }

  template <typename T, typename ExceptionType>
  static Reference<Flowable<T>> error(const ExceptionType& ex) {
    auto lambda = [ex = std::move(ex)](
        Reference<Subscriber<T>> subscriber, int64_t) {
      subscriber->onError(std::move(ex));
      return std::make_tuple(static_cast<int64_t>(0), true);
    };
    return Flowable<T>::create(std::move(lambda));
  }

  template <typename T, typename TGenerator>
  static Reference<Flowable<T>> fromGenerator(TGenerator generator) {
    auto lambda = [generator = std::move(generator)](
        Reference<Subscriber<T>> subscriber, int64_t requested) {
      int64_t generated = 0;
      try {
        while (generated < requested) {
          subscriber->onNext(generator());
          ++generated;
        }
        return std::make_tuple(generated, false);
      } catch (const std::exception& ex) {
        subscriber->onError(
            folly::exception_wrapper(std::current_exception(), ex));
        return std::make_tuple(generated, true);
      } catch (...) {
        subscriber->onError(std::runtime_error("unknown error"));
        return std::make_tuple(generated, true);
      }
    };
    return Flowable<T>::create(std::move(lambda));
  }

 private:
  Flowables() = delete;
};

} // flowable
} // yarpl
