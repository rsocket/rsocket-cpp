// Copyright 2004-present Facebook. All Rights Reserved.

#include "test/simple/StatsPrinter.h"
#include <folly/ExceptionWrapper.h>

namespace reactivesocket {
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
    reactivesocket::DuplexConnection* connection) {
  LOG(INFO) << "connectionCreated " << type;
}

void StatsPrinter::duplexConnectionClosed(
    const std::string& type,
    reactivesocket::DuplexConnection* connection) {
  LOG(INFO) << "connectionClosed " << type;
}

void StatsPrinter::bytesWritten(size_t bytes) {
  LOG(INFO) << "bytesWritten " << bytes;
}

void StatsPrinter::bytesRead(size_t bytes) {
  LOG(INFO) << "bytesRead " << bytes;
}

void StatsPrinter::frameWritten(const std::string& frameType) {
  LOG(INFO) << "frameWritten " << frameType;
}

void StatsPrinter::frameRead(const std::string& frameType) {
  LOG(INFO) << "frameRead " << frameType;
}

void StatsPrinter::resumeBufferChanged(
    int framesCountDelta,
    int dataSizeDelta) {
  LOG(INFO) << "resumeBufferChanged framesCountDelta=" << framesCountDelta
            << " dataSizeDelta=" << dataSizeDelta;
}

void StatsPrinter::connectionClosedInServerConnectionAcceptor(
    const folly::exception_wrapper& ex) {
  LOG(ERROR) << "connectionClosedInServerConnectionAcceptor ex=" << ex.what();
}
}
