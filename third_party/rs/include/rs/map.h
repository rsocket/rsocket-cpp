// Copyright 2017 Per Grön. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

#include <type_traits>

#include <rs/element_count.h>
#include <rs/publisher.h>
#include <rs/subscriber.h>
#include <rs/subscription.h>
#include <rs/weak_reference.h>

namespace shk {
namespace detail {

template <typename InnerSubscriberType, typename Mapper>
class MapSubscriber : public Subscriber {
 public:
  MapSubscriber(InnerSubscriberType &&inner_subscriber, const Mapper &mapper)
      : inner_subscriber_(std::move(inner_subscriber)),
        mapper_(mapper) {}

  void TakeSubscription(WeakReference<PureVirtualSubscription> &&subscription) {
    subscription_ = std::move(subscription);
  }

  template <typename T, class = RequireRvalue<T>>
  void OnNext(T &&t) {
    if (failed_) {
      return;
    }

    try {
      // We're only interested in catching the exception from mapper_ here, not
      // OnNext. But the specification requires that OnNext does not throw, and
      // here we rely on that.
      inner_subscriber_.OnNext(mapper_(std::forward<T>(t)));
    } catch (...) {
      if (subscription_) {
        // If !subscription_, then the underlying subscription has been
        // destroyed and is by definition already cancelled so there is nothing
        // to do.
        subscription_->Cancel();
      }
      failed_ = true;
      inner_subscriber_.OnError(std::current_exception());
    }
  }

  void OnError(std::exception_ptr &&error) {
    if (!failed_) {
      inner_subscriber_.OnError(std::move(error));
    }
  }

  void OnComplete() {
    if (!failed_) {
      inner_subscriber_.OnComplete();
    }
  }

 private:
  bool failed_ = false;
  InnerSubscriberType inner_subscriber_;
  WeakReference<PureVirtualSubscription> subscription_;
  Mapper mapper_;
};

}  // namespace detail

/**
 * Map is like the functional map operator that operates on a Publisher.
 */
template <typename Mapper>
auto Map(Mapper &&mapper) {
  // Return an operator (it takes a Publisher and returns a Publisher)
  return [mapper = std::forward<Mapper>(mapper)](auto source) {
    // Return a Publisher
    return MakePublisher([mapper, source = std::move(source)](
        auto &&subscriber) {
      using MapSubscriberT = detail::MapSubscriber<
          typename std::decay<decltype(subscriber)>::type,
          typename std::decay<Mapper>::type>;

      WeakReference<MapSubscriberT> map_ref;
      auto map_subscriber = WithWeakReference(
          MapSubscriberT(
              std::forward<decltype(subscriber)>(subscriber),
              mapper),
          &map_ref);

      WeakReference<PureVirtualSubscription> sub_ref;
      auto sub = WithWeakReference(
          MakeVirtualSubscription(source.Subscribe(std::move(map_subscriber))),
          &sub_ref);

      if (map_ref) {
        map_ref->TakeSubscription(std::move(sub_ref));
      }
      return sub;
    });
  };
}

}  // namespace shk
