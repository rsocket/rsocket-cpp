// Copyright 2004-present Facebook. All Rights Reserved.

#include <folly/io/async/ScopedEventBaseThread.h>
#include <gmock/gmock.h>
#include "src/SubscriberBase.h"
#include "test/ReactiveStreamsMocksCompat.h"

using namespace ::testing;
using namespace ::reactivesocket;

class SubscriberBaseMock : public SubscriberBaseT<int> {
 public:
  SubscriberBaseMock() : ExecutorBase(defaultExecutor()) {}
  explicit SubscriberBaseMock(folly::Executor& executor)
      : ExecutorBase(executor) {}

  MOCK_METHOD1(
      onSubscribeImpl_,
      void(std::shared_ptr<Subscription> subscription));
  MOCK_METHOD1(onNextImpl_, void(int value));
  MOCK_METHOD0(onCompleteImpl_, void());
  MOCK_METHOD1_T(onErrorImpl_, void(folly::exception_wrapper ex));

 private:
  void onSubscribeImpl(
      std::shared_ptr<Subscription> subscription) override final {
    onSubscribeImpl_(subscription);
  }

  void onNextImpl(int value) override final {
    onNextImpl_(value);
  }

  void onCompleteImpl() override final {
    onCompleteImpl_();
  }

  void onErrorImpl(folly::exception_wrapper ex) override final {
    onErrorImpl_(std::move(ex));
  }
};

TEST(SubscriberBaseTest, NoSignalAfterCancel) {
  auto subscriber = std::make_shared<SubscriberBaseMock>();

  std::shared_ptr<Subscription> outSubscription;
  EXPECT_CALL(*subscriber, onSubscribeImpl_(_))
      .WillOnce(Invoke([&](std::shared_ptr<Subscription> s) {
        outSubscription = s;
        s->cancel();
      }));

  EXPECT_CALL(*subscriber, onNextImpl_(_)).Times(0);
  EXPECT_CALL(*subscriber, onCompleteImpl_()).Times(0);
  EXPECT_CALL(*subscriber, onErrorImpl_(_)).Times(0);

  auto subscription = std::make_shared<MockSubscription>();
  std::shared_ptr<Subscriber<int>> ptr = subscriber;
  ptr->onSubscribe(subscription);

  ptr->onNext(1);
  ptr->onNext(2);
  ptr->onNext(3);
  ptr->onComplete();
  ptr->onError(std::runtime_error("error"));
}

TEST(SubscriberBaseTest, OnNextDelivered) {
  folly::ScopedEventBaseThread thread2;
  std::atomic<bool> done{false};

  auto subscriber =
      std::make_shared<SubscriberBaseMock>(*thread2.getEventBase());

  std::shared_ptr<Subscription> outSubscription;
  EXPECT_CALL(*subscriber, onSubscribeImpl_(_))
      .WillOnce(Invoke(
          [&](std::shared_ptr<Subscription> s) { outSubscription = s; }));

  EXPECT_CALL(*subscriber, onNextImpl_(_)).Times(3);
  EXPECT_CALL(*subscriber, onCompleteImpl_()).WillOnce(Invoke([&]() {
    done = true;
  }));
  EXPECT_CALL(*subscriber, onErrorImpl_(_)).Times(0);

  auto subscription = std::make_shared<MockSubscription>();
  std::shared_ptr<Subscriber<int>> ptr = subscriber;
  ptr->onSubscribe(subscription);

  ptr->onNext(1);
  ptr->onNext(2);
  ptr->onNext(3);
  ptr->onComplete();

  while (!done)
    ;
}

TEST(SubscriberBaseTest, CancelStopsOnNext) {
  folly::ScopedEventBaseThread thread2;
  std::atomic<bool> done{false};

  auto subscriber =
      std::make_shared<SubscriberBaseMock>(*thread2.getEventBase());

  std::shared_ptr<Subscription> outSubscription;
  EXPECT_CALL(*subscriber, onSubscribeImpl_(_))
      .WillOnce(Invoke(
          [&](std::shared_ptr<Subscription> s) { outSubscription = s; }));

  EXPECT_CALL(*subscriber, onNextImpl_(_))
      .WillOnce(Invoke([&](int) {}))
      .WillOnce(Invoke([&](int) {
        done = true;
        outSubscription->cancel();
      }));
  EXPECT_CALL(*subscriber, onCompleteImpl_()).Times(0);
  EXPECT_CALL(*subscriber, onErrorImpl_(_)).Times(0);

  auto subscription = std::make_shared<MockSubscription>();
  std::shared_ptr<Subscriber<int>> ptr = subscriber;
  ptr->onSubscribe(subscription);

  ptr->onNext(1);
  ptr->onNext(2);
  ptr->onNext(3);

  while (!done)
    ;
}

TEST(SubscriberBaseTest, SubscriptionRequest) {
  auto subscriber = std::make_shared<SubscriberBaseMock>();

  std::shared_ptr<Subscription> outSubscription;
  EXPECT_CALL(*subscriber, onSubscribeImpl_(_))
      .WillOnce(
          Invoke([&](std::shared_ptr<Subscription> s) { s->request(0); }));

  auto originalSubscription = std::make_shared<StrictMock<MockSubscription>>();
  EXPECT_CALL(*originalSubscription, request_(_)).Times(1);
  EXPECT_CALL(*originalSubscription, cancel_()).Times(1);

  auto subscription = std::make_shared<MockSubscription>();
  std::shared_ptr<Subscriber<int>> ptr = subscriber;
  ptr->onSubscribe(originalSubscription);
  ptr->onComplete();
}

TEST(SubscriberBaseTest, SubscriptionCancel) {
  auto subscriber = std::make_shared<SubscriberBaseMock>();

  std::shared_ptr<Subscription> outSubscription;
  EXPECT_CALL(*subscriber, onSubscribeImpl_(_))
      .WillOnce(Invoke([&](std::shared_ptr<Subscription> s) { s->cancel(); }));

  auto originalSubscription = std::make_shared<StrictMock<MockSubscription>>();
  EXPECT_CALL(*originalSubscription, cancel_()).Times(1);

  auto subscription = std::make_shared<MockSubscription>();
  std::shared_ptr<Subscriber<int>> ptr = subscriber;
  ptr->onSubscribe(originalSubscription);
}
