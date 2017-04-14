#pragma once

#include <limits>

#include "Subscriber.h"

namespace yarpl {

class Subscribers {
public:
  template<typename T, typename N>
  static auto create(N&& next, int64_t batch = Flowable<T>::NO_FLOW_CONTROL) {
    class Derived : public Subscriber<T> {
    public:
      Derived(N&& next, int64_t batch)
        : next_(std::forward<N>(next)), batch_(batch), pending_(0) {}

      virtual void onSubscribe(Reference<Subscription> subscription) override {
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

    return Reference<Derived>(new Derived(std::forward<N>(next), batch));
  }

private:
  Subscribers() = delete;
};

}  // yarpl
