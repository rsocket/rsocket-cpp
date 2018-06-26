// Copyright (c) Facebook, Inc. and its affiliates.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

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
    rsocket::DuplexConnection*) {
  LOG(INFO) << "connectionCreated " << type;
}

void StatsPrinter::duplexConnectionClosed(
    const std::string& type,
    rsocket::DuplexConnection*) {
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

void StatsPrinter::keepaliveSent() {
  LOG(INFO) << "keepalive sent";
}

void StatsPrinter::keepaliveReceived() {
  LOG(INFO) << "keepalive response received";
}
} // namespace rsocket
