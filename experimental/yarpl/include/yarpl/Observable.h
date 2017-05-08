#pragma once

#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

#include "yarpl/Scheduler.h"
#include "yarpl/utils/type_traits.h"

#include "Refcounted.h"
#include "observable/Observer.h"
#include "observable/Subscription.h"

#include "Flowable.h"
#include "Flowables.h"
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

  template <
      typename OnSubscribe,
      typename = typename std::enable_if<
          std::is_callable<OnSubscribe(Reference<Observer<T>>), void>::value>::
          type>
  static auto create(OnSubscribe&& function) {
    return Reference<Observable<T>>(new FromPublisherOperator<OnSubscribe>(
        std::forward<OnSubscribe>(function)));
  }

  template <typename Function>
  auto map(Function&& function);

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

 private:
  template <typename OnSubscribe>
  class FromPublisherOperator : public Observable<T> {
   public:
    FromPublisherOperator(OnSubscribe&& function)
        : function_(std::move(function)) {}

    void subscribe(Reference<Observer<T>> subscriber) {
      function_(std::move(subscriber));
    }

   private:
    OnSubscribe function_;
  };
};
} // observable
} // yarpl

#include "observable/ObservableOperator.h"

namespace yarpl {
namespace observable {
template <typename T>
template <typename Function>
auto Observable<T>::map(Function&& function) {
  using D = typename std::result_of<Function(T)>::type;
  return Reference<Observable<D>>(new MapOperator<T, D, Function>(
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
