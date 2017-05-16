#pragma once

#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

#include "yarpl/Scheduler.h"
#include "yarpl/utils/type_traits.h"

#include "../Refcounted.h"
#include "Observer.h"
#include "Subscription.h"

#include "../Flowable.h"
#include "yarpl/flowable/sources/Flowable_FromObservable.h"

namespace yarpl {
namespace observable {

/**
*Strategy for backpressure when converting from Observable to Flowable.
*/
enum class BackpressureStrategy { DROP };

template <typename T>
class Observable : public virtual Refcounted {
 public:
  static const auto CANCELED = std::numeric_limits<int64_t>::min();
  static const auto NO_FLOW_CONTROL = std::numeric_limits<int64_t>::max();

  virtual void subscribe(Reference<Observer<T>>) = 0;

  template <typename OnSubscribe>
  static auto create(OnSubscribe&&);

  template <typename Function>
  auto map(Function&& function);

  template <typename Function>
  auto filter(Function&& function);

  auto take(int64_t);

  auto subscribeOn(Scheduler&);

  /**
  * Convert from Observable to Flowable with a given BackpressureStrategy.
  *
  * Currently the only strategy is DROP.
  *
  * @param strategy
  * @return
  */
  auto toFlowable(BackpressureStrategy strategy);
};
} // observable
} // yarpl

#include "ObservableOperator.h"

namespace yarpl {
namespace observable {

template <typename T>
template <typename OnSubscribe>
auto Observable<T>::create(OnSubscribe&& function) {
  static_assert(
      std::is_callable<OnSubscribe(Reference<Observer<T>>), void>(),
      "OnSubscribe must have type `void(Reference<Observer<T>>)`");

  return make_ref<FromPublisherOperator<T, OnSubscribe>>(
      std::forward<OnSubscribe>(function));
}

template <typename T>
template <typename Function>
auto Observable<T>::map(Function&& function) {
  using D = typename std::result_of<Function(T)>::type;
  return Reference<Observable<D>>(new MapOperator<T, D, Function>(
      Reference<Observable<T>>(this), std::forward<Function>(function)));
}

template <typename T>
template <typename Function>
auto Observable<T>::filter(Function&& function) {
  return Reference<Observable<T>>(new FilterOperator<T, Function>(
      Reference<Observable<T>>(this), std::forward<Function>(function)));
}

template <typename T>
auto Observable<T>::take(int64_t limit) {
  return Reference<Observable<T>>(
      new TakeOperator<T>(Reference<Observable<T>>(this), limit));
}

template <typename T>
auto Observable<T>::subscribeOn(Scheduler& scheduler) {
  return Reference<Observable<T>>(
      new SubscribeOnOperator<T>(Reference<Observable<T>>(this), scheduler));
}

template <typename T>
auto Observable<T>::toFlowable(BackpressureStrategy strategy) {
  // we currently ONLY support the DROP strategy
  // so do not use the strategy parameter for anything
  auto o = Reference<Observable<T>>(this);
  return yarpl::flowable::Flowables::fromPublisher<T>([
    o = std::move(o), // the Observable to pass through
    strategy
  ](Reference<yarpl::flowable::Subscriber<T>> s) {
    s->onSubscribe(Reference<yarpl::flowable::Subscription>(
        new yarpl::flowable::sources::FlowableFromObservableSubscription<T>(
            std::move(o), std::move(s))));
  });
}

} // observable
} // yarpl
