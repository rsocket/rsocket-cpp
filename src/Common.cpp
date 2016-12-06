// Copyright 2004-present Facebook. All Rights Reserved.

#include "src/Common.h"
#include <folly/io/IOBuf.h>
#include "src/AbstractStreamAutomaton.h"

namespace reactivesocket {

static const char* getTerminatingSignalErrorMessage(int terminatingSignal) {
  switch ((StreamCompletionSignal)terminatingSignal) {
    case StreamCompletionSignal::CONNECTION_END:
      return "connection closed";
    case StreamCompletionSignal::CONNECTION_ERROR:
      return "connection error";
    case StreamCompletionSignal::ERROR:
      return "general error";
    case StreamCompletionSignal::GRACEFUL:
      DCHECK(false) << "throwing exception for GRACEFUL termination?";
      return "gracefull termination";
    default:
      return "stream interrupted";
  }
}

StreamInterruptedException::StreamInterruptedException(int _terminatingSignal)
    : std::runtime_error(getTerminatingSignalErrorMessage(_terminatingSignal)),
      terminatingSignal(_terminatingSignal) {}

} // reactivesocket
