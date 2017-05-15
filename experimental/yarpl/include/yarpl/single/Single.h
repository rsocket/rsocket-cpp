#pragma once

#include "yarpl/utils/type_traits.h"
#include "../Refcounted.h"
#include "SingleObserver.h"
#include "SingleSubscription.h"

namespace yarpl {
namespace single {

template <typename T>
class Single : public virtual Refcounted {
 public:
  static const auto CANCELED = std::numeric_limits<int64_t>::min();
  static const auto NO_FLOW_CONTROL = std::numeric_limits<int64_t>::max();

  virtual void subscribe(Reference<SingleObserver<T>>) = 0;

  template <
      typename OnSubscribe,
      typename = typename std::enable_if<std::is_callable<
          OnSubscribe(Reference<SingleObserver<T>>),
          void>::value>::type>
  static auto create(OnSubscribe&& function) {
    return Reference<Single<T>>(new FromPublisherOperator<OnSubscribe>(
        std::forward<OnSubscribe>(function)));
  }

  template <typename Function>
  auto map(Function&& function);

 private:
  template <typename OnSubscribe>
  class FromPublisherOperator : public Single<T> {
   public:
    explicit FromPublisherOperator(OnSubscribe&& function)
        : function_(std::move(function)) {}

    void subscribe(Reference<SingleObserver<T>> subscriber) override {
      function_(std::move(subscriber));
    }

   private:
    OnSubscribe function_;
  };
};
} // observable
} // yarpl

#include "SingleOperator.h"

namespace yarpl {
namespace single {
template <typename T>
template <typename Function>
auto Single<T>::map(Function&& function) {
  using D = typename std::result_of<Function(T)>::type;
  return Reference<Single<D>>(new MapOperator<T, D, Function>(
      Reference<Single<T>>(this), std::forward<Function>(function)));
}

} // single
} // yarpl
