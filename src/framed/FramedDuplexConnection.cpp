// Copyright 2004-present Facebook. All Rights Reserved.

#include "src/framed/FramedDuplexConnection.h"
#include "src/FrameSerializer.h"
#include "src/framed/FramedReader.h"
#include "src/framed/FramedWriter.h"

namespace reactivesocket {

FramedDuplexConnection::FramedDuplexConnection(
    std::unique_ptr<DuplexConnection> connection,
    folly::Executor& executor)
    : connection_(std::move(connection)),
      protocolVersion_(std::make_shared<ProtocolVersion>(
          FrameSerializer::getCurrentProtocolVersion())),
      executor_(executor) {}

FramedDuplexConnection::~FramedDuplexConnection() {
  // to make sure we close the parties when the connection dies
  if (outputWriter_) {
    outputWriter_->cancel();
  }
  if (inputReader_) {
    inputReader_->onComplete();
  }
}

std::shared_ptr<Subscriber<std::unique_ptr<folly::IOBuf>>>
FramedDuplexConnection::getOutput() noexcept {
  if (!outputWriter_) {
    outputWriter_ = std::make_shared<FramedWriter>(
        connection_->getOutput(), executor_, protocolVersion_);
  }
  return outputWriter_;
}

void FramedDuplexConnection::setInput(
    std::shared_ptr<Subscriber<std::unique_ptr<folly::IOBuf>>> framesSink) {
  CHECK(!inputReader_);
  inputReader_ = std::make_shared<FramedReader>(
      std::move(framesSink), executor_, protocolVersion_);
  connection_->setInput(inputReader_);
}

} // reactivesocket
