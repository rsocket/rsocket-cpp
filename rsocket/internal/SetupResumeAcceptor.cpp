// Copyright 2004-present Facebook. All Rights Reserved.

#include "rsocket/internal/SetupResumeAcceptor.h"

#include <folly/ExceptionWrapper.h>
#include <folly/io/async/EventBase.h>

#include "rsocket/framing/Frame.h"
#include "rsocket/framing/FrameProcessor.h"
#include "rsocket/framing/FrameSerializer.h"
#include "rsocket/framing/FrameTransportImpl.h"

namespace rsocket {

namespace {

/// FrameProcessor that does nothing.  Necessary to tell a FrameTransport it can
/// output frames in the cases where we want to error it.
class NoneFrameProcessor final : public FrameProcessor {
  void processFrame(std::unique_ptr<folly::IOBuf>) override {}
  void onTerminal(folly::exception_wrapper) override {}
};

/// Closes a DuplexConnection with an error.
void closeWithError(
    std::unique_ptr<DuplexConnection> connection,
    std::string message) {
  auto output = connection->getOutput();
  output->onSubscribe(yarpl::flowable::Subscription::empty());
  output->onError(std::runtime_error{std::move(message)});
}

/// Closes a DuplexConnection with an error, sending a serialized frame first.
void closeWithError(
    std::unique_ptr<DuplexConnection> connection,
    std::unique_ptr<folly::IOBuf> frame,
    std::string message) {
  auto output = connection->getOutput();
  output->onSubscribe(yarpl::flowable::Subscription::empty());
  output->onNext(std::move(frame));
  output->onError(std::runtime_error{std::move(message)});
}

} // namespace

SetupResumeAcceptor::OneFrameSubscriber::OneFrameSubscriber(
    SetupResumeAcceptor& acceptor,
    std::unique_ptr<DuplexConnection> connection,
    SetupResumeAcceptor::OnSetup onSetup,
    SetupResumeAcceptor::OnResume onResume)
    : acceptor_{acceptor},
      connection_{std::move(connection)},
      onSetup_{std::move(onSetup)},
      onResume_{std::move(onResume)} {
  DCHECK(connection_);
  DCHECK(onSetup_);
  DCHECK(onResume_);
  DCHECK(acceptor_.inOwnerThread());
}

void SetupResumeAcceptor::OneFrameSubscriber::setInput() {
  DCHECK(acceptor_.inOwnerThread());
  connection_->setInput(ref_from_this(this));
}

void SetupResumeAcceptor::OneFrameSubscriber::close() {
  auto self = ref_from_this(this);
  connection_.reset();
}

void SetupResumeAcceptor::OneFrameSubscriber::onSubscribeImpl() {
  DCHECK(acceptor_.inOwnerThread());
  this->request(std::numeric_limits<int32_t>::max());
}

void SetupResumeAcceptor::OneFrameSubscriber::onNextImpl(
    std::unique_ptr<folly::IOBuf> buf) {
  DCHECK(connection_) << "OneFrameSubscriber received more than one frame";
  DCHECK(acceptor_.inOwnerThread());

  this->cancel(); // calls onTerminateImpl

  acceptor_.processFrame(
      std::move(connection_),
      std::move(buf),
      std::move(onSetup_),
      std::move(onResume_));
}

void SetupResumeAcceptor::OneFrameSubscriber::onCompleteImpl() {}
void SetupResumeAcceptor::OneFrameSubscriber::onErrorImpl(
    folly::exception_wrapper) {}

void SetupResumeAcceptor::OneFrameSubscriber::onTerminateImpl() {
  DCHECK(acceptor_.inOwnerThread());
  acceptor_.remove(ref_from_this(this));
}

SetupResumeAcceptor::SetupResumeAcceptor(folly::EventBase* eventBase)
    : eventBase_{eventBase} {
  CHECK(eventBase_);
}

SetupResumeAcceptor::~SetupResumeAcceptor() {
  close().get();
}

void SetupResumeAcceptor::processFrame(
    std::unique_ptr<DuplexConnection> connection,
    std::unique_ptr<folly::IOBuf> buf,
    SetupResumeAcceptor::OnSetup onSetup,
    SetupResumeAcceptor::OnResume onResume) {
  DCHECK(inOwnerThread());
  DCHECK(connection);

  if (closed_) {
    std::string msg{"SetupResumeAcceptor is shutting down"};
    closeWithError(std::move(connection), std::move(msg));
    return;
  }

  auto serializer = FrameSerializer::createAutodetectedSerializer(*buf);
  if (!serializer) {
    std::string msg{"Unable to detect protocol version"};
    VLOG(2) << msg;
    closeWithError(std::move(connection), std::move(msg));
    return;
  }

  switch (serializer->peekFrameType(*buf)) {
    case FrameType::SETUP: {
      Frame_SETUP frame;
      if (!serializer->deserializeFrom(frame, std::move(buf))) {
        std::string msg{"Cannot decode SETUP frame"};
        auto err = serializer->serializeOut(Frame_ERROR::connectionError(msg));
        closeWithError(std::move(connection), std::move(err), std::move(msg));
        break;
      }

      VLOG(3) << "In: " << frame;

      SetupParameters params;
      frame.moveToSetupPayload(params);

      if (serializer->protocolVersion() != params.protocolVersion) {
        std::string msg{"SETUP frame has invalid protocol version"};
        auto err = serializer->serializeOut(Frame_ERROR::invalidSetup(msg));
        closeWithError(std::move(connection), std::move(err), std::move(msg));
        break;
      }

      auto transport =
          yarpl::make_ref<FrameTransportImpl>(std::move(connection));

      try {
        onSetup(transport, std::move(params));
      } catch (const std::exception& exn) {
        folly::exception_wrapper ew{std::current_exception(), exn};
        auto err = Frame_ERROR::rejectedSetup(ew.what().toStdString());
        transport->setFrameProcessor(std::make_shared<NoneFrameProcessor>());
        transport->outputFrameOrDrop(serializer->serializeOut(std::move(err)));
        transport->closeWithError(std::move(ew));
      }
      break;
    }

    case FrameType::RESUME: {
      Frame_RESUME frame;
      if (!serializer->deserializeFrom(frame, std::move(buf))) {
        std::string msg{"Cannot decode RESUME frame"};
        auto err = serializer->serializeOut(Frame_ERROR::connectionError(msg));
        closeWithError(std::move(connection), std::move(err), std::move(msg));
        break;
      }

      VLOG(3) << "In: " << frame;

      ResumeParameters params(
          std::move(frame.token_),
          frame.lastReceivedServerPosition_,
          frame.clientPosition_,
          ProtocolVersion(frame.versionMajor_, frame.versionMinor_));

      if (serializer->protocolVersion() != params.protocolVersion) {
        std::string msg{"RESUME frame has invalid protocol version"};
        auto err = serializer->serializeOut(Frame_ERROR::rejectedResume(msg));
        closeWithError(std::move(connection), std::move(err), std::move(msg));
        break;
      }

      auto transport =
          yarpl::make_ref<FrameTransportImpl>(std::move(connection));

      try {
        onResume(transport, std::move(params));
      } catch (const std::exception& exn) {
        folly::exception_wrapper ew{std::current_exception(), exn};
        auto err = Frame_ERROR::rejectedResume(ew.what().toStdString());
        transport->setFrameProcessor(std::make_shared<NoneFrameProcessor>());
        transport->outputFrameOrDrop(serializer->serializeOut(std::move(err)));
        transport->closeWithError(std::move(ew));
      }
      break;
    }

    default: {
      std::string msg{"Invalid frame, expected SETUP/RESUME"};
      auto err = serializer->serializeOut(Frame_ERROR::connectionError(msg));
      closeWithError(std::move(connection), std::move(err), std::move(msg));
      break;
    }
  }
}

void SetupResumeAcceptor::accept(
    std::unique_ptr<DuplexConnection> connection,
    OnSetup onSetup,
    OnResume onResume) {
  DCHECK(inOwnerThread());

  if (closed_) {
    return;
  }

  auto subscriber = yarpl::make_ref<OneFrameSubscriber>(
      *this, std::move(connection), std::move(onSetup), std::move(onResume));
  connections_.insert(subscriber);
  subscriber->setInput();
}

void SetupResumeAcceptor::remove(
    const yarpl::Reference<SetupResumeAcceptor::OneFrameSubscriber>&
        subscriber) {
  DCHECK(inOwnerThread());
  connections_.erase(subscriber);
}

folly::Future<folly::Unit> SetupResumeAcceptor::close() {
  if (inOwnerThread()) {
    closeAll();
    return folly::makeFuture();
  }
  return folly::via(eventBase_, [this] { closeAll(); });
}

void SetupResumeAcceptor::closeAll() {
  DCHECK(inOwnerThread());

  closed_ = true;

  auto connections = std::move(connections_);
  for (auto& connection : connections) {
    connection->close();
  }
}

bool SetupResumeAcceptor::inOwnerThread() const {
  return eventBase_->isInEventBaseThread();
}

} // namespace rsocket
