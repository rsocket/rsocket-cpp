// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <gmock/gmock.h>

#include <rsocket/internal/ManageableConnection.h>

namespace rsocket {

class MockManageableConnection : public rsocket::ManageableConnection {
 public:
  MOCK_METHOD0(listenCloseEvent_, folly::Future<folly::Unit>());
  MOCK_METHOD1(onClose_, void(folly::exception_wrapper));
  MOCK_METHOD2(close_, void(folly::exception_wrapper, StreamCompletionSignal));

  folly::Future<folly::Unit> listenCloseEvent() override {
    listenCloseEvent_();
    return closePromise_.getFuture();
  }

  /// Terminates underlying connection.
  ///
  /// This may synchronously deliver terminal signals to all
  /// StreamAutomatonBase attached to this ConnectionAutomaton.
  void close(folly::exception_wrapper ex, StreamCompletionSignal scs) override {
    closed_ = true;
    close_(ex, scs);
    onClose(ex);
  }

  bool isDisconnectedOrClosed() const override {
    return closed_ || disconnected_;
  }

 protected:
  void onClose(folly::exception_wrapper ex) override {
    onClose_(ex);
    closePromise_.setValue();
  }

 public:
  bool disconnected_{false};
  bool closed_{false};
};

} // namespace rsocket
