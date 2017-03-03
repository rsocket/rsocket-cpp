#pragma once

#include <memory>

namespace reactivestreams {

template<typename T>
class Subscriber;

template<typename T>
class Publisher {
public:
  virtual ~Publisher() = default;
  virtual void subscribe(std::unique_ptr<Subscriber<T>> subscriber) = 0;
};

}  // reactivestreams
