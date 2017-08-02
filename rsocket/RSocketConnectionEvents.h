// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

namespace folly {
class exception_wrapper;
}

namespace rsocket {

// The application should implement this interface to get called-back
// on network events.
class RSocketConnectionEvents {
 public:
  virtual ~RSocketConnectionEvents() = default;

  // This method gets called when the underlying transport is connected to the
  // remote side.  This does not necessarily mean that the RSocket connection
  // will be successful.  As an example, the transport might get reconnected
  // for an existing RSocketStateMachine.  But resumption at the RSocket layer
  // might not succeed.
  virtual void onConnected() {}

  // This gets called when the underlying transport has disconnected.  This also
  // means the RSocket connection is disconnected.
  virtual void onDisconnected(const folly::exception_wrapper&) {}

  // This gets called when the RSocketStateMachine is closed.  You cant use this
  // RSocketStateMachine anymore.
  virtual void onClosed(const folly::exception_wrapper&) {}

  // This gets called when no more frames can be sent over the RSocket streams.
  // This typically happens immediately after onDisconnected(). The streams can
  // be resumed after onStreamsResumed() event.
  virtual void onStreamsPaused() {}

  // This gets called when the underlying transport has been successfully
  // connected AND the connection can be resumed at the RSocket layer.  This
  // typically gets called after onConnected()
  virtual void onStreamsResumed() {}
};
}
