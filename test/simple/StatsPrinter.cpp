// Copyright 2004-present Facebook. All Rights Reserved.

#include "StatsPrinter.h"

#include <glog/logging.h>
#include <src/tcp/TcpDuplexConnection.h>

namespace reactivesocket {
void StatsPrinter::socketCreated() {
  LOG(INFO) << "socketCreated";
}

void StatsPrinter::socketClosed() {
  LOG(INFO) << "socketClosed";
}

void StatsPrinter::connectionCreated(
    const std::string& type,
    reactivesocket::DuplexConnection* connection) {
  LOG(INFO) << "connectionCreated " << type;
}

void StatsPrinter::connectionClosed(
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

void StatsPrinter::frameWritten(FrameType type) {
  LOG(INFO) << "frameWritten " << type;
}

void StatsPrinter::frameRead(FrameType type) {
  LOG(INFO) << "frameRead " << type;
}
}