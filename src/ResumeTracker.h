// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include "src/Common.h"

namespace folly {
class IOBuf;
}

namespace reactivesocket {

class ResumeTracker {
 public:
  void trackReceivedFrame(const folly::IOBuf& serializedFrame);
  static bool shouldTrackFrame(const folly::IOBuf& serializedFrame);

  ResumePosition impliedPosition() {
    return impliedPosition_;
  }

 private:
  ResumePosition impliedPosition_{0};
};
}
