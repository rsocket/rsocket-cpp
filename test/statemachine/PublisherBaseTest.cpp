// Copyright 2004-present Facebook. All Rights Reserved.

#include "test/deprecated/ReactiveSocket.h"
#include "src/temporary_home/SubscriberBase.h"
#include "src/statemachine/PublisherBase.h"
#include "test/test_utils/MockRequestHandler.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace ::testing;
using namespace ::reactivesocket;
using namespace yarpl;

namespace {

class UserSubscriber : public yarpl::flowable::Subscriber<Payload>, private PublisherBase {
 public:
  UserSubscriber() : PublisherBase(5) {}
  using PublisherBase::pausePublisherStream;
  using PublisherBase::publisherSubscribe;

  void onSubscribe(
      yarpl::Reference<yarpl::flowable::Subscription> subscription) noexcept override {
    publisherSubscribe(std::move(subscription));
  }

  void onNext(::reactivesocket::Payload element) noexcept override {
    FAIL();

  }

  void onComplete() noexcept override {
    FAIL();
  }

  void onError(const std::exception_ptr) noexcept override {
    FAIL();
  }
};

class UserSubscription : public yarpl::flowable::Subscription {
  void request(int64_t n) noexcept override {
    EXPECT_EQ(5ll, n);
  }

  void cancel() noexcept override {
    FAIL();
  }
};
}

TEST(PublisherBaseTest, GetsPassedOriginalSubscription) {
  MockRequestHandler requestHandler;
  auto subscription = make_ref<UserSubscription>();
  auto userSubscriber = make_ref<UserSubscriber>();

  EXPECT_CALL(requestHandler, onSubscriptionPaused_(Eq(subscription)));

  userSubscriber->onSubscribe(subscription);
  userSubscriber->pausePublisherStream(requestHandler);
  userSubscriber.reset();
}
