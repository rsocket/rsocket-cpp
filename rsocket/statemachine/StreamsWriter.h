// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include "rsocket/Payload.h"
#include "rsocket/internal/Common.h"

namespace rsocket {

/// The interface for writing stream related frames on the wire.
class StreamsWriter {
 public:
  virtual ~StreamsWriter() = default;

  virtual void writeNewStream(
      StreamId streamId,
      StreamType streamType,
      uint32_t initialRequestN,
      Payload payload,
      bool TEMP_completed) = 0;

  virtual void writeRequestN(Frame_REQUEST_N&&) = 0;
  virtual void writeCancel(Frame_CANCEL&&) = 0;

  virtual void writePayload(Frame_PAYLOAD&&) = 0;
  virtual void writeError(Frame_ERROR&&) = 0;

  virtual void onStreamClosed(StreamId, StreamCompletionSignal) = 0;
};
}
