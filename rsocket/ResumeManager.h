// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <folly/Optional.h>

#include "rsocket/framing/Frame.h"
#include "rsocket/framing/FrameTransport.h"

namespace folly {
class IOBuf;
}

namespace rsocket {

// Applications desiring to have cold-resumption should implement a
// ResumeManager interface.  By default, an in-memory implementation of this
// interface (InMemResumeManager) will be used by RSocket.
//
// The API refers to the stored frames by "position".  "position" is the byte
// count at frame boundaries.  For example, if the ResumeManager has stored 3
// 100-byte sent frames starting from byte count 150.  Then,
// - isPositionAvailable would return true for the values [150, 250, 350].
// - firstSentPosition() would return 150
// - lastSentPosition() would return 350
class ResumeManager {
 public:
  virtual ~ResumeManager(){};

  // The following methods will be called for each frame which is being
  // sent/received on the wire.  The application should implement a way to
  // store the sent and received frames in persistent storage.
  virtual void trackReceivedFrame(
      const folly::IOBuf& serializedFrame,
      FrameType frameType,
      StreamId streamId) = 0;

  virtual void trackSentFrame(
      const folly::IOBuf& serializedFrame,
      FrameType frameType,
      folly::Optional<StreamId> streamIdPtr) = 0;

  // We have received acknowledgement from the remote-side that it has frames
  // up to "position".  We can discard all frames before that.  This
  // information is periodically received from remote-side through KeepAlive
  // frames.
  virtual void resetUpToPosition(ResumePosition position) = 0;

  // The application should check its persistent storage and respond whether it
  // has frames starting from "position" in send buffer.
  virtual bool isPositionAvailable(ResumePosition position) const = 0;

  // The application should send frames starting from the "position" using the
  // provided "transport".  As an alternative, we could design the API such
  // that we retrieve individual frames from the application and send them over
  // wire.  But that would mean application has random access to frames
  // indexed by position.  This API gives the flexibility to the application to
  // store the frames in any way it wants (randomly accessed or sequentially
  // accessed).
  virtual void sendFramesFromPosition(
      ResumePosition position,
      FrameTransport& transport) const = 0;

  // This should return the first (oldest) available position in the send
  // buffer.
  virtual ResumePosition firstSentPosition() const = 0;

  // This should return the last (latest) available position in the send
  // buffer.
  virtual ResumePosition lastSentPosition() const = 0;

  // This should return the latest tracked position of frames received from
  // remote side.
  virtual ResumePosition impliedPosition() const = 0;

  // This gets called when a new stream is opened.
  virtual void onStreamOpen(StreamId streamId, FrameType frameType) = 0;

  // This gets called when a stream is closed.
  virtual void onStreamClosed(StreamId streamId) = 0;
};
}
