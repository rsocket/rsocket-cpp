#pragma once

#include <limits>

#include "Observable.h"

namespace yarpl {
namespace observable {

class Observables {
 public:
  static Reference<Observable<int64_t>> range(int64_t start, int64_t end) {
    auto lambda = [ start, end, i = start ](
        Subscriber<int64_t> & subscriber, int64_t requested) mutable {
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

    return Observable<int64_t>::create(std::move(lambda));
  }

  template <typename T>
  static Reference<Observable<T>> just(const T& value) {
    auto lambda = [value](Subscriber<T>& subscriber, int64_t) {
      // # requested should be > 0.  Ignoring the actual parameter.
      subscriber.onNext(value);
      subscriber.onComplete();
      return std::make_tuple(static_cast<int64_t>(1), true);
    };

    return Observable<T>::create(std::move(lambda));
  }

  template <typename T>
  static Reference<Observable<T>> just(std::initializer_list<T> list) {
    auto lambda = [ list, it = list.begin() ](
        Subscriber<T> & subscriber, int64_t requested) mutable {
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

    return Observable<T>::create(std::move(lambda));
  }

  template <
      typename T,
      typename OnSubscribe,
      typename = typename std::enable_if<std::is_callable<
          OnSubscribe(Reference<Subscriber<T>>),
          void>::value>::type>

  static Reference<Observable<T>> create(OnSubscribe&& function) {
    return Reference<Observable<T>>(new FromPublisherOperator<T, OnSubscribe>(
        std::forward<OnSubscribe>(function)));
  }

 private:
  Observables() = delete;
};

} // observable
} // yarpl
