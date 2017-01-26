// Copyright 2004-present Facebook. All Rights Reserved.
#include "FramedDuplexConnection.h"

#include "FramedReader.h"
#include "FramedWriter.h"

namespace reactivesocket {

FramedDuplexConnection::FramedDuplexConnection(
    std::unique_ptr<DuplexConnection> connection)
    : connection_(std::move(connection)) {}

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
    outputWriter_ = std::make_shared<FramedWriter>(connection_->getOutput());
  }
  return outputWriter_;
}

void FramedDuplexConnection::setInput(
    std::shared_ptr<Subscriber<std::unique_ptr<folly::IOBuf>>> framesSink) {
  CHECK(!inputReader_);
  inputReader_ = std::make_shared<FramedReader>(std::move(framesSink));
  connection_->setInput(inputReader_);
}

} // reactivesocket
