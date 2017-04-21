// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

//  /**
//   * Lift an operator into F<T> and return F<R>
//   * @tparam R
//   * @tparam F
//   * @param onSubscribeLift
//   * @return
//   */
//  template <
//      typename R,
//      typename F,
//      typename = typename std::enable_if<std::is_callable<
//          F(std::unique_ptr<reactivestreams_yarpl::Subscriber<R>>),
//        std::unique_ptr<reactivestreams_yarpl::Subscriber<T>>>::value>::type>
//  std::unique_ptr<FlowableV<R>> lift(F&& onSubscribeLift) {
//    return FlowableV<R>::create(
//        [ this, onSub = std::move(onSubscribeLift) ](auto sOfR) mutable {
//          this->subscribe(std::move(onSub(std::move(sOfR))));
//        });
//  }

//  /**
//   * Map F<T> -> F<R>
//   *
//   * @tparam F
//   * @param function
//   * @return
//   */
//  template <
//      typename F,
//      typename = typename std::enable_if<
//         std::is_callable<F(T), typename std::result_of<F(T)>::type>::value>::
//          type>
//  std::unique_ptr<FlowableV<typename std::result_of<F(T)>::type>> map(
//      F&& function);

//  /**
//   * Take n items from F<T> then cancel.
//   * @param toTake
//   * @return
//   */
//  std::unique_ptr<FlowableV<T>> take(int64_t toTake) {
//    return lift<T>(yarpl::operators::FlowableTakeOperator<T>(toTake));
//  }

//  /**
//   * SubscribeOn the given Scheduler
//   * @param scheduler
//   * @return
//   */
//  std::unique_ptr<FlowableV<T>> subscribeOn(yarpl::Scheduler& scheduler) {
// return lift<T>(yarpl::operators::FlowableSubscribeOnOperator<T>(scheduler));
//  }

// protected:
//  FlowableV() = default;

// template <typename T>
// template <typename F, typename Default>
// std::unique_ptr<FlowableV<typename std::result_of<F(T)>::type>>
// FlowableV<T>::map(F&& function) {
//  return lift<typename std::result_of<F(T)>::type>(
//      yarpl::operators::
//          FlowableMapOperator<T, typename std::result_of<F(T)>::type, F>(
//              std::forward<F>(function)));
//}
