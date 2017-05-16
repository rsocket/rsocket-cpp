// Copyright 2004-present Facebook. All Rights Reserved.

#include <gtest/gtest.h>
#include "src/FrameTransport.h"
#include "src/NullRequestHandler.h"
#include "test/InlineConnection.h"

using namespace ::testing;
using namespace ::reactivesocket;

TEST(FrameTransportTest, OnSubscribeAfterClose) {
  class NullSubscription : public reactivesocket::Subscription {
   public:
    // Subscription methods
    void request(size_t n) noexcept override {}
    void cancel() noexcept override {}
  };

  FrameTransport transport(std::make_unique<InlineConnection>());
  transport.close(std::runtime_error("test_close"));
  static_cast<Subscriber<std::unique_ptr<folly::IOBuf>>&>(transport)
      .onSubscribe(std::make_shared<NullSubscription>());
  // if we got here, we passed all the checks in the onSubscribe method
}
