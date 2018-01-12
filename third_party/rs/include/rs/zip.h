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

#include <bitset>
#include <memory>
#include <type_traits>

#include <rs/detail/optional.h>
#include <rs/element_count.h>
#include <rs/publisher.h>
#include <rs/subscriber.h>
#include <rs/subscription.h>
#include <rs/weak_reference.h>

namespace shk {
namespace detail {

template <size_t Idx>
struct TupleMapHelper {
  template <typename Tuple, typename Mapper>
  static void Map(Tuple &&tuple, Mapper &&mapper) {
    using DecayedTuple = typename std::decay<Tuple>::type;
    mapper(std::get<std::tuple_size<DecayedTuple>::value - Idx>(tuple));
    TupleMapHelper<Idx - 1>::Map(
        std::forward<Tuple>(tuple), std::forward<Mapper>(mapper));
  }
};

template <>
struct TupleMapHelper<0> {
  template <typename Tuple, typename Mapper>
  static void Map(Tuple &&, Mapper &&) {}
};

/**
 * Takes a tuple and a mapper function and invokes the mapper on each of the
 * elements of the tuple.
 */
template <typename Tuple, typename Mapper>
void TupleMap(Tuple &&tuple, Mapper &&mapper) {
  using DecayedTuple = typename std::decay<Tuple>::type;
  TupleMapHelper<std::tuple_size<DecayedTuple>::value>::Map(
      std::forward<Tuple>(tuple), std::forward<Mapper>(mapper));
}

template <
    size_t Idx,
    typename Tuple,
    template <typename> class Map,
    typename ...Types>
struct TupleTypeMapHelper {
  using Type =
      typename TupleTypeMapHelper<
          Idx - 1,
          Tuple,
          Map,
          Types...,
          Map<std::tuple_element_t<Idx - 1, Tuple>>>::Type;
};

template <
    typename Tuple,
    template <typename> class Map,
    typename ...Types>
struct TupleTypeMapHelper<0, Tuple, Map, Types...> {
  using Type = std::tuple<Types...>;
};

/**
 * Type-level function: Takes a tuple and a Map type function and returns a
 * tuple where each element type has been mapped with the Map function.
 */
template <typename Tuple, template <typename> class Map>
using TupleTypeMap = typename TupleTypeMapHelper<
    std::tuple_size<Tuple>::value, Tuple, Map>::Type;

template <typename T>
using WrapInUniquePtr = std::unique_ptr<T>;

template <typename Tuple, typename InnerSubscriberType, typename ...Publishers>
class ZipSubscription : public Subscription {
  // The buffer is a tuple of deques, one for each input stream.
  using Buffer = TupleTypeMap<Tuple, WrapInUniquePtr>;
  using FinishedSubscriptions = std::bitset<sizeof...(Publishers)>;

  template <size_t Idx>
  class ZipSubscriber : public Subscriber {
   public:
    explicit ZipSubscriber(
        WeakReference<ZipSubscription> &&zip_subscription)
        : zip_subscription_(std::move(zip_subscription)) {}

    void OnNext(std::tuple_element_t<Idx, Tuple> &&elm) {
      if (zip_subscription_) {
        (*zip_subscription_).template OnInnerSubscriptionNext<Idx>(
            std::move(elm));
      }
    }

    void OnError(std::exception_ptr &&error) {
      if (zip_subscription_) {
        zip_subscription_->OnInnerSubscriptionError(std::move(error));
      }
    }

    void OnComplete() {
      if (zip_subscription_) {
        zip_subscription_->template OnInnerSubscriptionComplete<Idx>();
      }
    }

   private:
    // This can be a weak reference because if the ZipSubscription is destroyed,
    // then the subscription is cancelled by definition and it's okay to not
    // deliver signals.
    WeakReference<ZipSubscription> zip_subscription_;
  };

  template <typename>
  using ToSubscription = AnySubscription;
  using Subscriptions = TupleTypeMap<Tuple, ToSubscription>;

  template <size_t Idx, typename ...InnerPublishers>
  class ZipSubscriptionBuilder {
   public:
    template <
        typename SubscriptionReferee,
        typename ...AccumulatedSubscriptions>
    static auto Subscribe(
        SubscriptionReferee &&zip_subscription,
        const std::tuple<Publishers...> &publishers,
        AccumulatedSubscriptions &...accumulated_subscriptions) {
      static_assert(
          IsPublisher<std::tuple_element_t<Idx, std::tuple<Publishers...>>>,
          "Zip must be given a Publisher");

      WeakReference<ZipSubscription> subscription_ref;
      auto wrapped_zip_subscription = WithWeakReference(
          std::move(zip_subscription),
          &subscription_ref);

      auto new_subscription = AnySubscription(
          std::get<Idx>(publishers).Subscribe(
              ZipSubscriber<Idx>(std::move(subscription_ref))));
      return ZipSubscriptionBuilder<Idx + 1, InnerPublishers...>::Subscribe(
          std::move(wrapped_zip_subscription),
          publishers,
          accumulated_subscriptions...,
          new_subscription);
    }
  };

  template <typename ...InnerPublishers>
  class ZipSubscriptionBuilder<
      sizeof...(InnerPublishers), InnerPublishers...> {
   public:
    template <
        typename SubscriptionReferee,
        typename ...AccumulatedSubscriptions>
    static auto Subscribe(
        SubscriptionReferee &&zip_subscription,
        const std::tuple<Publishers...> &publishers,
        AccumulatedSubscriptions &...accumulated_subscriptions) {
      zip_subscription.subscriptions_ = std::make_unique<Subscriptions>(
              std::move(accumulated_subscriptions)...);
      return std::move(zip_subscription);
    }
  };

 public:
  ZipSubscription() = default;

  explicit ZipSubscription(InnerSubscriberType &&inner_subscriber)
      : inner_subscriber_(std::move(inner_subscriber)) {}

  template <typename Subscriber>
  static auto Subscribe(
      Subscriber &&subscriber,
      const std::tuple<Publishers...> &publishers) {
    ZipSubscription me(std::forward<Subscriber>(subscriber));

    auto wrapped_me = ZipSubscriptionBuilder<0, Publishers...>::Subscribe(
        me, publishers);
    if (wrapped_me.finished_) {
      wrapped_me.subscriptions_.reset();
    }

    if (sizeof...(Publishers) == 0) {
      wrapped_me.SendOnComplete();
    }

    return std::move(wrapped_me);
  }

  void Request(ElementCount count) {
    if (!finished_ && inner_subscriber_) {
      requested_ += count;

      if (values_pending_.count() == 0 && requested_ > 0) {
        values_pending_.set();
        TupleMap(*subscriptions_, [](auto &&subscription) {
          subscription.Request(ElementCount(1));
        });
      }
    }
  }

  void Cancel() {
    finished_ = true;
    if (subscriptions_) {
      TupleMap(*subscriptions_, [](auto &&subscription) {
        subscription.Cancel();
      });
    }
  }

 private:
  template <size_t Idx>
  void OnInnerSubscriptionNext(std::tuple_element_t<Idx, Tuple> &&element) {
    if (finished_) {
      return;
    }

    auto &buffered_element = std::get<Idx>(buffer_);
    if (buffered_element || !values_pending_[Idx]) {
      OnInnerSubscriptionError(std::make_exception_ptr(std::logic_error(
          "Backpressure violation")));
    } else {
      values_pending_[Idx] = false;
      buffered_element.reset(
          new std::tuple_element_t<Idx, Tuple>(std::move(element)));

      if (values_pending_.count() == 0) {
        Emit();
      }
    }
  }

  void OnInnerSubscriptionError(std::exception_ptr &&error) {
    if (!finished_) {
      Cancel();
      inner_subscriber_->OnError(std::move(error));
    }
  }

  template <size_t Idx>
  void OnInnerSubscriptionComplete() {
    finished_subscriptions_.set(Idx);
    if (!std::get<Idx>(buffer_)) {
      // Only if the buffer for this stream is empty is it safe to send
      // OnComplete here. If the buffer is non-empty there is a chance that
      // elements will arrive on the other streams and then the buffer should
      // be used.
      SendOnComplete();
    }
  }

  void SendOnComplete() {
    if (!finished_) {
      finished_ = true;
      inner_subscriber_->OnComplete();
    }
  }

  template <size_t Idx, typename Dummy>
  class EmittedElementBuilder {
   public:
    template <
        typename Buffer,
        typename ...AccumulatedElements>
    static Tuple Build(
        const FinishedSubscriptions &finished_subscriptions,
        bool *should_finish,
        Buffer *buffer,
        AccumulatedElements &...accumulated_elements) {
      auto &buffered_element = std::get<Idx>(*buffer);
      auto element = std::move(*buffered_element);
      buffered_element.reset();

      if (finished_subscriptions[Idx]) {
        // We have encountered a finish stream that has an empty buffer. This
        // means that this Zip stream will never emit any more elements. It's
        // time to cancel the others.
        *should_finish = true;
      }

      return EmittedElementBuilder<Idx + 1, Dummy>::Build(
          finished_subscriptions,
          should_finish,
          buffer,
          accumulated_elements...,
          element);
    }
  };

  template <typename Dummy>
  class EmittedElementBuilder<sizeof...(Publishers), Dummy> {
   public:
    template <
        typename Buffer,
        typename ...AccumulatedElements>
    static Tuple Build(
        const FinishedSubscriptions &finished_subscriptions,
        bool *should_finish,
        Buffer *buffer,
        AccumulatedElements &...accumulated_elements) {
      return Tuple(std::move(accumulated_elements)...);
    }
  };

  void Emit() {
    bool should_finish = false;
    inner_subscriber_->OnNext(EmittedElementBuilder<0, int>::Build(
        finished_subscriptions_, &should_finish, &buffer_));
    if (should_finish) {
      SendOnComplete();
    } else {
      --requested_;
      Request(ElementCount(0));
    }
  }

  std::unique_ptr<Subscriptions> subscriptions_;
  FinishedSubscriptions finished_subscriptions_{};
  std::bitset<sizeof...(Publishers)> values_pending_{};
  bool finished_ = false;
  detail::Optional<InnerSubscriberType> inner_subscriber_;
  Buffer buffer_;
  ElementCount requested_;
};

}  // namespace detail

/**
 * Zip takes a number of input streams and returns a stream of tuples containing
 * elements from all input streams combined.
 *
 * If the input streams emit different numbers of elements, the resulting stream
 * emits as many values as the smallest input stream. The other values are
 * dropped.
 */
template <typename Tuple, typename ...Publishers>
auto Zip(Publishers &&...publishers) {
  static_assert(
      std::tuple_size<Tuple>::value == sizeof...(Publishers),
      "Zip tuple type must have as many elements as there are Publishers");

  return MakePublisher([
      publishers = std::make_tuple(std::forward<Publishers>(publishers)...)](
          auto &&subscriber) {
    using ZipSubscriptionT = detail::ZipSubscription<
        Tuple,
        typename std::decay<decltype(subscriber)>::type,
        typename std::decay<Publishers>::type...>;

    return ZipSubscriptionT::Subscribe(
        std::forward<decltype(subscriber)>(subscriber),
        publishers);
  });
}

}  // namespace shk
