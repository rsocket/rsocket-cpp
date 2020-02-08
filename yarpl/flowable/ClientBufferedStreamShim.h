// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once
#if FOLLY_HAS_COROUTINES

#include <folly/experimental/coro/Baton.h>
#include <folly/experimental/coro/Invoke.h>
#include <folly/experimental/coro/Task.h>
#include <thrift/lib/cpp2/async/ClientBufferedStream.h>
#include <yarpl/flowable/Flowable.h>

namespace yarpl {
namespace flowable {
class ClientBufferedStreamShim {
 public:
  template <typename T>
  static std::shared_ptr<yarpl::flowable::Flowable<T>> create(
      apache::thrift::ClientBufferedStream<T>&& stream,
      folly::Executor::KeepAlive<folly::SequencedExecutor> ex) {
    struct SharedState {
      SharedState(
          apache::thrift::detail::ClientStreamBridge::ClientPtr streamBridge,
          folly::Executor::KeepAlive<folly::SequencedExecutor> ex)
          : streamBridge_(std::move(streamBridge)), ex_(std::move(ex)) {}
      apache::thrift::detail::ClientStreamBridge::Ptr streamBridge_;
      folly::Executor::KeepAlive<folly::SequencedExecutor> ex_;
      std::atomic<bool> canceled_{false};
    };

    return yarpl::flowable::internal::flowableFromSubscriber<T>(
        [state =
             std::make_shared<SharedState>(std::move(stream.streamBridge_), ex),
         decode =
             stream.decode_](std::shared_ptr<yarpl::flowable::Subscriber<T>>
                                 subscriber) mutable {
          class Subscription : public yarpl::flowable::Subscription {
           public:
            explicit Subscription(std::weak_ptr<SharedState> state)
                : state_(std::move(state)) {}

            void request(int64_t n) override {
              CHECK(n != yarpl::credits::kNoFlowControl)
                  << "kNoFlowControl unsupported";

              if (auto state = state_.lock()) {
                state->ex_->add([n, state = std::move(state)]() {
                  state->streamBridge_->requestN(n);
                });
              }
            }

            void cancel() override {
              if (auto state = state_.lock()) {
                state->ex_->add([state = std::move(state)]() {
                  state->streamBridge_->cancel();
                  state->canceled_ = true;
                });
              }
            }

           private:
            std::weak_ptr<SharedState> state_;
          };

          state->ex_->add([keepAlive = state->ex_.copy(),
                           subscriber,
                           subscription = std::make_shared<Subscription>(
                               std::weak_ptr<SharedState>(state))]() mutable {
            subscriber->onSubscribe(std::move(subscription));
          });

          folly::coro::co_invoke(
              [subscriber = std::move(subscriber),
               state,
               decode]() mutable -> folly::coro::Task<void> {
                apache::thrift::detail::ClientStreamBridge::ClientQueue queue;
                class ReadyCallback
                    : public apache::thrift::detail::ClientStreamConsumer {
                 public:
                  void consume() override {
                    baton.post();
                  }

                  void canceled() override {
                    baton.post();
                  }

                  folly::coro::Baton baton;
                };

                while (!state->canceled_) {
                  if (queue.empty()) {
                    ReadyCallback callback;
                    if (state->streamBridge_->wait(&callback)) {
                      co_await callback.baton;
                    }
                    queue = state->streamBridge_->getMessages();
                    if (queue.empty()) {
                      // we've been cancelled
                      apache::thrift::detail::ClientStreamBridge::Ptr(
                          state->streamBridge_.release());
                      break;
                    }
                  }

                  {
                    auto& payload = queue.front();
                    if (!payload.hasValue() && !payload.hasException()) {
                      state->ex_->add(
                          [subscriber = std::move(subscriber),
                           keepAlive = state->ex_.copy()]() mutable {
                            subscriber->onComplete();
                          });
                      break;
                    }
                    auto value = decode(std::move(payload));
                    queue.pop();
                    if (value.hasValue()) {
                      state->ex_->add([subscriber,
                                       keepAlive = state->ex_.copy(),
                                       value = std::move(value)]() mutable {
                        subscriber->onNext(std::move(value).value());
                      });
                    } else if (value.hasException()) {
                      state->ex_->add([subscriber = std::move(subscriber),
                                       keepAlive = state->ex_.copy(),
                                       value = std::move(value)]() mutable {
                        subscriber->onError(std::move(value).exception());
                      });
                      break;
                    } else {
                      LOG(FATAL) << "unreachable";
                    }
                  }
                }
              })
              .scheduleOn(state->ex_)
              .start();
        });
  }
};
} // namespace flowable
} // namespace yarpl

#endif
