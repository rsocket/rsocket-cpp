// Copyright 2004-present Facebook. All Rights Reserved.

#include "StatsPrinter.h"
#include <glog/logging.h>

namespace rsocket {
void StatsPrinter::socketCreated() {
  LOG(INFO) << "socketCreated";
}

void StatsPrinter::socketClosed(StreamCompletionSignal /*signal*/) {
  LOG(INFO) << "socketClosed";
}

void StatsPrinter::socketDisconnected() {
  LOG(INFO) << "socketDisconnected";
}

void StatsPrinter::duplexConnectionCreated(
    const std::string& type,
    rsocket::DuplexConnection* connection) {
  LOG(INFO) << "connectionCreated " << type;
}

void StatsPrinter::duplexConnectionClosed(
    const std::string& type,
    rsocket::DuplexConnection* connection) {
  LOG(INFO) << "connectionClosed " << type;
}

void StatsPrinter::bytesWritten(size_t bytes) {
  LOG(INFO) << "bytesWritten " << bytes;
}

void StatsPrinter::bytesRead(size_t bytes) {
  LOG(INFO) << "bytesRead " << bytes;
}

void StatsPrinter::frameWritten(FrameType frameType) {
  LOG(INFO) << "frameWritten " << frameType;
}

void StatsPrinter::frameRead(FrameType frameType) {
  LOG(INFO) << "frameRead " << frameType;
}

void StatsPrinter::resumeBufferChanged(
    int framesCountDelta,
    int dataSizeDelta) {
  LOG(INFO) << "resumeBufferChanged framesCountDelta=" << framesCountDelta
            << " dataSizeDelta=" << dataSizeDelta;
}

void StatsPrinter::streamBufferChanged(
    int64_t framesCountDelta,
    int64_t dataSizeDelta) {
  LOG(INFO) << "streamBufferChanged framesCountDelta=" << framesCountDelta
            << " dataSizeDelta=" << dataSizeDelta;
}
}
