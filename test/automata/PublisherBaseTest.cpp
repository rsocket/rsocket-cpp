// Copyright 2004-present Facebook. All Rights Reserved.

#include "src/ReactiveSocket.h"
#include "src/SubscriberBase.h"
#include "src/automata/PublisherBase.h"
#include "test/MockRequestHandler.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace ::testing;
using namespace ::reactivesocket;

namespace {

class UserSubscriber : public SubscriberBase, private PublisherBase {
 public:
  UserSubscriber() : ExecutorBase(inlineExecutor()), PublisherBase(5) {}
  using PublisherBase::pausePublisherStream;
  using PublisherBase::publisherSubscribe;

 protected:
  void onSubscribeImpl(
      std::shared_ptr<Subscription> subscription) noexcept override {
    publisherSubscribe(std::move(subscription));
  }

  void onNextImpl(::reactivesocket::Payload element) noexcept override {
    FAIL();
  }

  void onCompleteImpl() noexcept override {
    FAIL();
  }

  void onErrorImpl(folly::exception_wrapper ex) noexcept override {
    FAIL();
  }
};

class UserSubscription : public Subscription {
  void request(size_t n) noexcept override {
    EXPECT_EQ(5ul, n);
  }

  void cancel() noexcept override {
    FAIL();
  }
};
}

TEST(PublisherBaseTest, GetsPassedOriginalSubscription) {
  MockRequestHandler requestHandler;
  auto subscription = std::make_shared<UserSubscription>();
  auto userSubscriber = std::make_shared<UserSubscriber>();

  EXPECT_CALL(requestHandler, onSubscriptionPaused_(Eq(subscription)));

  userSubscriber->onSubscribe(subscription);
  userSubscriber->pausePublisherStream(requestHandler);
  userSubscriber.reset();
}
