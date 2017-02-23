#pragma once

#include <yarpl/Flowable_Subscription.h>

namespace yarpl {
namespace flowable {

/**
 *
 */
template <typename T> class Subscriber {
public:
  virtual ~Subscriber(){};

  static std::unique_ptr<Subscriber<T>>
  create(std::function<void(const T &)> onNext);

  static std::unique_ptr<Subscriber<T>>
  create(std::function<void(const T &)> onNext,
         std::function<void(const std::exception &)> onError);

  static std::unique_ptr<Subscriber<T>>
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

template <typename T> class ObserverImpl : public Subscriber<T> {
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
    // default to requesting max() (infinite)
    // which means this Subscriber is synchronous and needs no flow control
    s->request(std::numeric_limits<uint64_t>::max());
  }
};

template <typename T>
std::unique_ptr<Subscriber<T>>
Subscriber<T>::create(std::function<void(const T &)> onNext) {
  return std::unique_ptr<Subscriber<T>>(new ObserverImpl<T>(onNext));
}

template <typename T>
std::unique_ptr<Subscriber<T>>
Subscriber<T>::create(std::function<void(const T &)> onNext,
                      std::function<void(const std::exception &)> onError) {
  return std::unique_ptr<Subscriber<T>>(new ObserverImpl<T>(onNext, onError));
}

template <typename T>
std::unique_ptr<Subscriber<T>>
Subscriber<T>::create(std::function<void(const T &)> onNext,
                      std::function<void(const std::exception &)> onError,
                      std::function<void()> onComplete) {
  return std::unique_ptr<Subscriber<T>>(
      new ObserverImpl<T>(onNext, onError, onComplete));
}

} // observable namespace
} // yarpl namespace
