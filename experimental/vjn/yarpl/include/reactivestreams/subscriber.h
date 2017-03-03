#pragma once

#include <exception>
#include <memory>

#include "type_traits.h"

namespace reactivestreams {

template <typename T>
class Subscription;

template <typename T>
class Subscriber {
 public:
  virtual ~Subscriber() = default;

  virtual void on_next(const T&) {}
  virtual void on_next(T&& value) { on_next(value); }
  virtual void on_error(const std::exception_ptr error) { throw error; }
  virtual void on_complete() {}
  virtual void on_subscribe(Subscription<T>*) {}

  template <typename Next,
            typename = typename std::enable_if<
                std::is_callable<Next(const T&), void>::value>::type>
  static std::unique_ptr<Subscriber<T>> create(Next&& next) {
    return std::unique_ptr<Subscriber<T>>(
        new Derived1<Next>(std::forward<Next>(next)));
  }

  template <
      typename Next, typename Error,
      typename = typename std::enable_if<
          std::is_callable<Next(const T&), void>::value &&
          std::is_callable<Error(const std::exception_ptr), void>::value>::type>
  static std::unique_ptr<Subscriber<T>> create(Next&& next, Error&& error) {
    return std::unique_ptr<Subscriber<T>>(new Derived2<Next, Error>(
        std::forward<Next>(next), std::forward<Error>(error)));
  }

  template <typename Next, typename Error, typename Complete,
            typename = typename std::enable_if<
                std::is_callable<Next(const T&), void>::value &&
                std::is_callable<Error(const std::exception_ptr), void>::value &&
                std::is_callable<Complete(), void>::value>::type>
  static std::unique_ptr<Subscriber<T>> create(Next&& next, Error&& error,
                                               Complete&& complete) {
    return std::unique_ptr<Subscriber<T>>(new Derived3<Next, Error, Complete>(
        std::forward<Next>(next), std::forward<Error>(error),
        std::forward<Complete>(complete)));
  }

 private:
  template <typename Next>
  class Derived1 : public Subscriber {
   public:
    Derived1(Next&& next) : next_(std::forward<Next>(next)) {}

    virtual void on_next(const T& value) { next_(value); }

   private:
    Next next_;
  };

  template <typename Next, typename Error>
  class Derived2 : public Derived1<Next> {
   public:
    Derived2(Next&& next, Error&& error)
        : Derived1<Next>(std::forward<Next>(next)),
          error_(std::forward<Error>(error)) {}

    virtual void on_error(const std::exception_ptr error) { error_(error); }

   private:
    Error error_;
  };

  template <typename Next, typename Error, typename Complete>
  class Derived3 : public Derived2<Next, Error> {
   public:
    Derived3(Next&& next, Error&& error, Complete&& complete)
        : Derived2<Next, Error>(std::forward<Next>(next),
                                std::forward<Error>(error)),
          complete_(std::forward<Complete>(complete)) {}

    virtual void on_complete() { complete_(); }

   private:
    Complete complete_;
  };
};

}  // reactivestreams
