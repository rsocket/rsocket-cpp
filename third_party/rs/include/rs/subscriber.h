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

#include <array>
#include <memory>
#include <tuple>
#include <type_traits>

namespace shk {

/**
 * Classes that conform to the Subscriber concept should inherit from this class
 * to signify that they are a Subscriber.
 *
 * Subscriber types must have the following methods:
 *
 * * void OnNext(T &&t); -- One such method for each type that the subscriber
 *   accepts.
 * * void OnError(std::exception_ptr &&error);
 * * void OnComplete();
 */
class Subscriber {
 protected:
  ~Subscriber() = default;
};

template <typename T>
constexpr bool IsSubscriber = std::is_base_of<Subscriber, T>::value;

template <typename T>
constexpr bool IsRvalue =
    !std::is_lvalue_reference<T>::value &&
    !(std::is_rvalue_reference<T>::value && std::is_const<T>::value);

template <typename T>
using RequireRvalue = typename std::enable_if<IsRvalue<T>>::type;

namespace detail {

class EmptySubscriber : public Subscriber {
 public:
  EmptySubscriber() = default;

  EmptySubscriber(const EmptySubscriber &) = delete;
  EmptySubscriber& operator=(const EmptySubscriber &) = delete;

  EmptySubscriber(EmptySubscriber &&) = default;
  EmptySubscriber& operator=(EmptySubscriber &&) = default;

  template <typename T, class = RequireRvalue<T>>
  void OnNext(T &&t) {}
  void OnError(std::exception_ptr &&error);
  void OnComplete();
};

template <typename OnNextCb, typename OnErrorCb, typename OnCompleteCb>
class CallbackSubscriber : public Subscriber {
 public:
  CallbackSubscriber(
      OnNextCb &&on_next, OnErrorCb &&on_error, OnCompleteCb &&on_complete)
      : on_next_(std::forward<OnNextCb>(on_next)),
        on_error_(std::forward<OnErrorCb>(on_error)),
        on_complete_(std::forward<OnCompleteCb>(on_complete)) {}

  template <typename T, class = RequireRvalue<T>>
  void OnNext(T &&t) {
    on_next_(std::forward<T>(t));
  }

  void OnError(std::exception_ptr &&error) {
    on_error_(std::move(error));
  }

  void OnComplete() {
    on_complete_();
  }

 private:
  OnNextCb on_next_;
  OnErrorCb on_error_;
  OnCompleteCb on_complete_;
};

template <typename ...Ts>
class ErasedSubscriberBase : public Subscriber {
 public:
  template <
      typename S,
      class = typename std::enable_if<IsSubscriber<
          typename std::remove_reference<S>::type>>::type>
  explicit ErasedSubscriberBase(S &&s)
      : eraser_(std::make_unique<SubscriberEraser<
            typename std::decay<S>::type>>(std::forward<S>(s))) {}

  void OnError(std::exception_ptr &&error) {
    eraser_->OnError(std::move(error));
  }

  void OnComplete() {
    eraser_->OnComplete();
  }

 protected:
  void OnNextWithPointer(int index, void *rvalue_ref) {
    eraser_->OnNext(index, rvalue_ref);
  }

 private:
  class OnNextEraser {
   public:
    virtual ~OnNextEraser() = default;

    virtual void OnNext(void *object, void *rvalue_ref) const = 0;
  };

  template <typename Object, typename Parameter>
  class MethodOnNextEraser : public OnNextEraser {
   public:
    using Method = void (Object::*)(Parameter &&);

    MethodOnNextEraser(Method method)
        : method_(method) {}

    void OnNext(void *object, void *rvalue_ref) const override {
      (reinterpret_cast<Object *>(object)->*method_)(
          std::move(*reinterpret_cast<Parameter *>(rvalue_ref)));
    }

   private:
    Method method_;
  };

  class Eraser {
   public:
    virtual ~Eraser() = default;

    virtual void OnNext(int index, void *rvalue_ref) = 0;
    virtual void OnError(std::exception_ptr &&error) = 0;
    virtual void OnComplete() = 0;
  };

  template <typename S>
  class SubscriberEraser : public Eraser {
   public:
    template <typename UniversalSReference>
    SubscriberEraser(UniversalSReference &&subscriber)
        : subscriber_(std::forward<UniversalSReference>(subscriber)) {}

    void OnNext(int index, void *rvalue_ref) override {
      (*GetNexts()[index]).OnNext(&subscriber_, rvalue_ref);
    }

    void OnError(std::exception_ptr &&error) override {
      subscriber_.OnError(std::move(error));
    }

    void OnComplete() override {
      subscriber_.OnComplete();
    }

   private:
    using Nexts = std::array<
        std::unique_ptr<const OnNextEraser>, sizeof...(Ts)>;

    const Nexts &GetNexts() {
      static Nexts nexts = MakeNexts();
      return nexts;
    }

    template <size_t Idx, typename Dummy>
    class NextsBuilder {
     public:
      static void AssignNexts(Nexts *nexts) {
        using Parameter = std::tuple_element_t<Idx, std::tuple<Ts...>>;

        void (S::*method)(Parameter &&) = &S::OnNext;

        (*nexts)[Idx].reset(new MethodOnNextEraser<S, Parameter>(method));
        NextsBuilder<Idx + 1, Dummy>::AssignNexts(nexts);
      }
    };

    template <typename Dummy>
    class NextsBuilder<sizeof...(Ts), Dummy> {
     public:
      static void AssignNexts(Nexts *nexts) {
      }
    };

    static Nexts MakeNexts() {
      Nexts result{};
      NextsBuilder<0, int>::AssignNexts(&result);
      return result;
    }

    // Sanity check to ensure that the user of this class didn't forget
    // std::decay
    static_assert(
        !std::is_reference<S>::value,
        "SubscriberEraser can't hold a reference type");
    S subscriber_;
  };

  std::unique_ptr<Eraser> eraser_;
};

/**
 * ErasedSubscriber is a class that does a templatey inheritance dance in order
 * to be able to have one OnNext method for each Ts type.
 */
template <size_t Idx, typename Base, typename ...Ts>
class ErasedSubscriber;

template <size_t Idx, typename Base, typename T, typename ...Ts>
class ErasedSubscriber<Idx, Base, T, Ts...>
    : public ErasedSubscriber<Idx + 1, Base, Ts...> {
 public:
  using ErasedSubscriber<Idx + 1, Base, Ts...>::OnNext;

  void OnNext(T &&val) {
    this->template OnNextWithPointer(Idx, static_cast<void *>(&val));
  }

 protected:
  template <
      typename S,
      class = typename std::enable_if<IsSubscriber<
          typename std::remove_reference<S>::type>>::type>
  explicit ErasedSubscriber(S &&s)
      : ErasedSubscriber<Idx + 1, Base, Ts...>(std::forward<S>(s)) {}
};

template <size_t Idx, typename Base>
class ErasedSubscriber<Idx, Base> : public Base {
  struct PrivateDummyType {};

 public:
  // This overload is here so that the using ...::OnNext above can work.
  void OnNext(PrivateDummyType &&);  // Intentionally not implemented

 protected:
  template <
      typename S,
      class = typename std::enable_if<IsSubscriber<
          typename std::remove_reference<S>::type>>::type>
  explicit ErasedSubscriber(S &&s)
      : Base(std::forward<S>(s)) {}
};

}  // namespace detail

/**
 * Type erasure wrapper for Subscriber objects.
 */
template <typename ...Ts>
class AnySubscriber : public detail::ErasedSubscriber<
    0, detail::ErasedSubscriberBase<Ts...>, Ts...> {
 public:
  template <
      typename S,
      class = typename std::enable_if<IsSubscriber<
          typename std::remove_reference<S>::type>>::type>
  explicit AnySubscriber(S &&s)
      : detail::ErasedSubscriber<0, detail::ErasedSubscriberBase<Ts...>, Ts...>(
            std::forward<S>(s)) {}
};

/**
 * Template specialization for the common single type case, to increase runtime
 * and compilation speed.
 */
template <typename T>
class AnySubscriber<T> : public Subscriber {
 public:
  template <
      typename S,
      class = typename std::enable_if<IsSubscriber<
          typename std::remove_reference<S>::type>>::type>
  explicit AnySubscriber(S &&s)
      : eraser_(std::make_unique<SubscriberEraser<
            typename std::decay<S>::type>>(std::forward<S>(s))) {}

  void OnNext(T &&val) {
    eraser_->OnNext(std::move(val));
  }

  void OnError(std::exception_ptr &&error) {
    eraser_->OnError(std::move(error));
  }

  void OnComplete() {
    eraser_->OnComplete();
  }

 private:
  class Eraser {
   public:
    virtual ~Eraser() = default;

    virtual void OnNext(T &&val) = 0;
    virtual void OnError(std::exception_ptr &&error) = 0;
    virtual void OnComplete() = 0;
  };

  template <typename S>
  class SubscriberEraser : public Eraser {
   public:
    template <typename UniversalSReference>
    SubscriberEraser(UniversalSReference &&subscriber)
        : subscriber_(std::forward<UniversalSReference>(subscriber)) {}

    void OnNext(T &&val) override {
      subscriber_.OnNext(std::move(val));
    }

    void OnError(std::exception_ptr &&error) override {
      subscriber_.OnError(std::move(error));
    }

    void OnComplete() override {
      subscriber_.OnComplete();
    }

   private:
    // Sanity check to ensure that the user of this class didn't forget
    // std::decay
    static_assert(
        !std::is_reference<S>::value,
        "SubscriberEraser can't hold a reference type");
    S subscriber_;
  };

  std::unique_ptr<Eraser> eraser_;
};

inline detail::EmptySubscriber MakeSubscriber() {
  return detail::EmptySubscriber();
}

template <typename OnNextCb, typename OnErrorCb, typename OnCompleteCb>
auto MakeSubscriber(
    OnNextCb &&on_next, OnErrorCb &&on_error, OnCompleteCb &&on_complete) {
  return detail::CallbackSubscriber<OnNextCb, OnErrorCb, OnCompleteCb>(
      std::forward<OnNextCb>(on_next),
      std::forward<OnErrorCb>(on_error),
      std::forward<OnCompleteCb>(on_complete));
}

}  // namespace shk
