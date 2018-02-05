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
#include <rs/subscription.h>

namespace shk {
namespace detail {

template <typename Container, typename Subscriber>
class FromSubscription : public Subscription {
 public:
  FromSubscription() = default;

  FromSubscription(
      const Container &container,
      Subscriber &&subscriber)
      : container_(std::make_unique<Container>(container)),
        subscriber_(std::make_unique<Subscriber>(
            std::move(subscriber))),
        it_(std::begin(*container_)),
        end_(std::end(*container_)) {
    if (it_ == end_) {
      subscriber_->OnComplete();
    }
  }

  FromSubscription(const FromSubscription &) = delete;
  FromSubscription& operator=(const FromSubscription &) = delete;

  FromSubscription(FromSubscription &&) = default;
  FromSubscription& operator=(FromSubscription &&) = default;

  void Request(ElementCount count) {
    if (!subscriber_) {
      return;
    }

    bool has_outstanding_request_count = outstanding_request_count_ != 0;
    outstanding_request_count_ += count;
    if (has_outstanding_request_count) {
      // Farther up in the stack, Request is already being called. No need
      // to do anything here.
      return;
    }

    while (!cancelled_ && outstanding_request_count_ != 0 && !(it_ == end_)) {
      auto &&value = *it_;
      ++it_;
      subscriber_->OnNext(std::move(value));
      if (it_ == end_) {
        outstanding_request_count_ = 0;  // Just for sanity, not needed
        subscriber_->OnComplete();
      }

      // Need to decrement this after calling OnNext/OnComplete, to ensure
      // that re-entrant Request calls always see that they are re-entrant.
      outstanding_request_count_--;
    }
  }

  void Cancel() {
    cancelled_ = true;
  }

 private:
  // container_ needs to be in a unique_ptr because this object is movable
  // and STL does not guarantee that the iterators will remain valid after
  // moving the container. Keeping it in a unique_ptr ensures that the
  // iterators stay valid even when this object is moved because the
  // container itself is never moved.
  std::unique_ptr<Container> container_;
  // TODO(peck): It would be nice to make this an optional instead of
  // unique_ptr; there is no need for this heap allocation.
  std::unique_ptr<Subscriber> subscriber_;
  decltype(std::begin(std::declval<Container &>())) it_;
  decltype(std::end(std::declval<Container &>())) end_;
  bool cancelled_ = false;
  ElementCount outstanding_request_count_ = ElementCount(0);
};
}  // namespace detail

/**
 * From takes a C++ container, for example an std::vector and returns a
 * Publisher that emits the values of that container.
 *
 * From makes a copy of the container every time the Publisher is subscribed
 * to, in order to be able to give ownership of each value in the container to
 * its Subscriber.
 */
template <typename Container>
auto From(Container &&container) {
  return MakePublisher([container = std::forward<Container>(container)](
      auto &&subscriber) {
    using DecayedContainer = typename std::decay<Container>::type;
    using DecayedSubscriber = typename std::decay<decltype(subscriber)>::type;

    return detail::FromSubscription<DecayedContainer, DecayedSubscriber>(
        container,
        std::forward<decltype(subscriber)>(subscriber));
  });
}

}  // namespace shk
