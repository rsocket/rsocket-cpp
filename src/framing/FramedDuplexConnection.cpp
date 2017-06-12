// Copyright 2004-present Facebook. All Rights Reserved.

#include "src/framing/FramedDuplexConnection.h"
#include "src/framing/FrameSerializer.h"
#include "src/framing/FramedReader.h"
#include "src/framing/FramedWriter.h"

namespace rsocket {

FramedDuplexConnection::FramedDuplexConnection(
    std::unique_ptr<DuplexConnection> connection,
    ProtocolVersion protocolVersion,
    folly::Executor& executor)
    : connection_(std::move(connection)),
      protocolVersion_(std::make_shared<ProtocolVersion>(protocolVersion)),
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
  outputWriter_ = std::make_shared<FramedWriter>(
      connection_->getOutput(), executor_, protocolVersion_);
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
