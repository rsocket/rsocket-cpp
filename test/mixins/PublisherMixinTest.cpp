// Copyright 2004-present Facebook. All Rights Reserved.

#include "src/StandardReactiveSocket.h"
#include "src/SubscriberBase.h"
#include "src/mixins/PublisherMixin.h"
#include "test/MockRequestHandler.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace ::testing;
using namespace ::reactivesocket;

namespace {

class UserSubscriber : public SubscriberBase, private PublisherMixin {
 public:
  UserSubscriber() : ExecutorBase(inlineExecutor()), PublisherMixin(5) {}
  using PublisherMixin::pausePublisherStream;
  using PublisherMixin::publisherSubscribe;

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
    EXPECT_EQ(5, n);
  }

  void cancel() noexcept override {
    FAIL();
  }
};
}

TEST(PublisherMixinTest, GetsPassedOriginalSubscription) {
  MockRequestHandler requestHandler;
  auto subscription = std::make_shared<UserSubscription>();
  auto userSubscriber = std::make_shared<UserSubscriber>();

  EXPECT_CALL(requestHandler, onSubscriptionPaused_(Eq(subscription)));

  userSubscriber->onSubscribe(subscription);
  userSubscriber->pausePublisherStream(requestHandler);
  userSubscriber.reset();
}
