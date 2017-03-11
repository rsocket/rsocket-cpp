// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <exception>
#include <memory>

namespace reactivestreams_yarpl {
// TODO remove 'yarpl' if we agree on this usage elsewhere

template <typename T>
class Subscription {
 public:
  virtual ~Subscription() = default;
  virtual void request(long n) = 0;
  virtual void cancel() = 0;
};

template <typename T>
class Subscriber {
 public:
  virtual ~Subscriber() = default;
  virtual void onSubscribe(Subscription<T>*) = 0;
  virtual void onNext(const T&) = 0;
  virtual void onNext(T&& value) {
    onNext(value);
  }
  virtual void onComplete() = 0;
  virtual void onError(const std::exception_ptr error) {
    throw error;
  }
};

template <typename T>
class Publisher {
 public:
  virtual ~Publisher() = default;
  virtual void subscribe(std::unique_ptr<Subscriber<T>>) = 0;
};
}