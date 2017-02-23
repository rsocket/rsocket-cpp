#pragma once

#include <yarpl/Observable_Subscription.h>

namespace yarpl {
namespace observable {

/**
 *
 */
template <typename T> class Observer {
public:
  static std::unique_ptr<Observer<T>>
  create(std::function<void(const T &)> onNext);

  static std::unique_ptr<Observer<T>>
  create(std::function<void(const T &)> onNext,
         std::function<void(const std::exception &)> onError);

  static std::unique_ptr<Observer<T>>
  create(std::function<void(const T &)> onNext,
         std::function<void(const std::exception &)> onError,
         std::function<void()> onComplete);

  virtual void onNext(const T &value) = 0;
  virtual void onError(const std::exception &e) = 0;
  virtual void onComplete() = 0;
  virtual void onSubscribe(std::unique_ptr<Subscription>) = 0;
};

/* ****************************************** */
/* implementation here because of templates */
/* https://isocpp.org/wiki/faq/templates#templates-defn-vs-decl */
/* ****************************************** */

static const std::function<void(const std::exception &)> DEFAULT_ERROR =
    [](const std::exception &) {};
static const std::function<void()> DEFAULT_COMPLETE = []() {};

template <typename T> class ObserverImpl : public Observer<T> {
  std::function<void(const T &)> next;
  std::function<void(const std::exception &)> error;
  std::function<void()> complete;

public:
  ObserverImpl(std::function<void(const T &)> n)
      : next(n), error(DEFAULT_ERROR), complete(DEFAULT_COMPLETE){};
  ObserverImpl(std::function<void(const T &)> n,
               std::function<void(const std::exception &)> e)
      : next(n), error(e), complete(DEFAULT_COMPLETE){};
  ObserverImpl(std::function<void(const T &)> n,
               std::function<void(const std::exception &)> e,
               std::function<void()> c)
      : next(n), error(e), complete(c){};

  void onNext(const T &value) { next.operator()(value); }

  void onError(const std::exception &e) { error.operator()(e); }

  void onComplete() { complete.operator()(); }

  void onSubscribe(std::unique_ptr<Subscription> s) {
    // do nothing with onSubscribe when lambdas used
    // cancellation is not supported in that case
  }
};

template <typename T>
std::unique_ptr<Observer<T>>
Observer<T>::create(std::function<void(const T &)> onNext) {
  return std::unique_ptr<Observer<T>>(new ObserverImpl<T>(onNext));
}

template <typename T>
std::unique_ptr<Observer<T>>
Observer<T>::create(std::function<void(const T &)> onNext,
                    std::function<void(const std::exception &)> onError) {
  return std::unique_ptr<Observer<T>>(new ObserverImpl<T>(onNext, onError));
}

template <typename T>
std::unique_ptr<Observer<T>>
Observer<T>::create(std::function<void(const T &)> onNext,
                    std::function<void(const std::exception &)> onError,
                    std::function<void()> onComplete) {
  return std::unique_ptr<Observer<T>>(
      new ObserverImpl<T>(onNext, onError, onComplete));
}

} // observable namespace
} // yarpl namespace
