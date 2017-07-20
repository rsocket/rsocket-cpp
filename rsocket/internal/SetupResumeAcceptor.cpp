// Copyright 2004-present Facebook. All Rights Reserved.

#include "rsocket/internal/SetupResumeAcceptor.h"

#include <folly/ExceptionWrapper.h>
#include <folly/io/async/EventBase.h>

#include "rsocket/DuplexConnection.h"
#include "rsocket/framing/Frame.h"
#include "rsocket/framing/FrameProcessor.h"
#include "rsocket/framing/FrameSerializer.h"
#include "rsocket/framing/FrameTransport.h"

namespace rsocket {

namespace {

folly::exception_wrapper error(folly::StringPiece message) {
  std::runtime_error exn{message.str()};
  auto eptr = std::make_exception_ptr(exn);
  return folly::exception_wrapper{std::move(eptr), exn};
}
}

class OneFrameProcessor : public FrameProcessor {
 public:
  OneFrameProcessor(
      SetupResumeAcceptor& acceptor,
      yarpl::Reference<FrameTransport> transport,
      SetupResumeAcceptor::OnSetup onSetup,
      SetupResumeAcceptor::OnResume onResume)
      : acceptor_(acceptor),
        transport_(std::move(transport)),
        onSetup_(std::move(onSetup)),
        onResume_(std::move(onResume)) {
    DCHECK(transport_);
    DCHECK(onSetup_);
    DCHECK(onResume_);
  }

  void processFrame(std::unique_ptr<folly::IOBuf> buf) override {
    acceptor_.processFrame(
        std::move(transport_),
        std::move(buf),
        std::move(onSetup_),
        std::move(onResume_));
    // No more code here as the instance might be gone by now.
  }

  void onTerminal(folly::exception_wrapper ew) override {
    onSetup_ = nullptr;
    onResume_ = nullptr;

    acceptor_.close(std::move(transport_), std::move(ew));
    // No more code here as the instance might be gone by now.
  }

 private:
  SetupResumeAcceptor& acceptor_;
  yarpl::Reference<FrameTransport> transport_;
  SetupResumeAcceptor::OnSetup onSetup_;
  SetupResumeAcceptor::OnResume onResume_;
};

SetupResumeAcceptor::SetupResumeAcceptor(
    ProtocolVersion version,
    folly::EventBase* eventBase)
    : eventBase_(eventBase) {
  CHECK(eventBase_);

  // If the version is unknown we'll try to autodetect it from the first frame.
  if (version != ProtocolVersion::Unknown) {
    defaultSerializer_ = FrameSerializer::createFrameSerializer(version);
  }
}

SetupResumeAcceptor::~SetupResumeAcceptor() {
  close().get();
}

void SetupResumeAcceptor::processFrame(
    yarpl::Reference<FrameTransport> transport,
    std::unique_ptr<folly::IOBuf> buf,
    SetupResumeAcceptor::OnSetup onSetup,
    SetupResumeAcceptor::OnResume onResume) {
  DCHECK(eventBase_->isInEventBaseThread());

  if (closed_) {
    transport->closeWithError(error("SetupResumeAcceptor is shutting down"));
    return;
  }

  auto serializer = createSerializer(*buf);
  if (!serializer) {
    close(std::move(transport), error("Unable to detect protocol version"));
    return;
  }

  switch (serializer->peekFrameType(*buf)) {
    case FrameType::SETUP: {
      Frame_SETUP frame;
      if (!serializer->deserializeFrom(frame, std::move(buf))) {
        transport->outputFrameOrEnqueue(
            serializer->serializeOut(Frame_ERROR::invalidFrame()));
        close(std::move(transport), error("Cannot decode SETUP frame"));
        break;
      }

      VLOG(3) << "In: " << frame;

      SetupParameters params;
      frame.moveToSetupPayload(params);

      if (serializer->protocolVersion() != params.protocolVersion) {
        constexpr folly::StringPiece message{
            "SETUP frame has invalid protocol version"};
        transport->outputFrameOrEnqueue(serializer->serializeOut(
            Frame_ERROR::badSetupFrame(message.str())));
        close(transport, error(message));
        break;
      }

      remove(transport);

      try {
        onSetup(transport, std::move(params));
      } catch (const std::exception& exn) {
        folly::exception_wrapper ew{std::current_exception(), exn};
        auto errFrame = Frame_ERROR::rejectedSetup(ew.what().toStdString());
        transport->outputFrameOrEnqueue(
            serializer->serializeOut(std::move(errFrame)));
        close(std::move(transport), std::move(ew));
      }
      break;
    }

    case FrameType::RESUME: {
      Frame_RESUME frame;
      if (!serializer->deserializeFrom(frame, std::move(buf))) {
        transport->outputFrameOrEnqueue(
            serializer->serializeOut(Frame_ERROR::invalidFrame()));
        close(std::move(transport), error("Cannot decode RESUME frame"));
        break;
      }

      VLOG(3) << "In: " << frame;

      ResumeParameters params(
          std::move(frame.token_),
          frame.lastReceivedServerPosition_,
          frame.clientPosition_,
          ProtocolVersion(frame.versionMajor_, frame.versionMinor_));

      if (serializer->protocolVersion() != params.protocolVersion) {
        constexpr folly::StringPiece message{
            "RESUME frame has invalid protocol version"};
        transport->outputFrameOrEnqueue(serializer->serializeOut(
            Frame_ERROR::badSetupFrame(message.str())));
        close(std::move(transport), error(message));
        break;
      }

      remove(transport);

      try {
        onResume(transport, std::move(params));
      } catch (const std::exception& exn) {
        folly::exception_wrapper ew{std::current_exception(), exn};
        auto errFrame = Frame_ERROR::rejectedResume(ew.what().toStdString());
        transport->outputFrameOrEnqueue(
            serializer->serializeOut(std::move(errFrame)));
        close(std::move(transport), std::move(ew));
      }
      break;
    }

    default: {
      transport->outputFrameOrEnqueue(
          serializer->serializeOut(Frame_ERROR::unexpectedFrame()));
      close(
          std::move(transport), error("Invalid frame, expected SETUP/RESUME"));
      break;
    }
  }
}

void SetupResumeAcceptor::accept(
    std::unique_ptr<DuplexConnection> connection,
    OnSetup onSetup,
    OnResume onResume) {
  auto transport = yarpl::make_ref<FrameTransport>(std::move(connection));
  auto processor = std::make_shared<OneFrameProcessor>(
      *this, transport, std::move(onSetup), std::move(onResume));
  connections_.insert(transport);
  // Transport can receive frames right away.
  transport->setFrameProcessor(std::move(processor));
}

std::shared_ptr<FrameSerializer> SetupResumeAcceptor::createSerializer(
    const folly::IOBuf& frame) {
  if (defaultSerializer_) {
    return defaultSerializer_;
  }

  auto serializer = FrameSerializer::createAutodetectedSerializer(frame);
  if (!serializer) {
    VLOG(2) << "Unable to detect protocol version";
    return nullptr;
  }

  VLOG(3) << "Detected protocol version " << serializer->protocolVersion();
  return std::move(serializer);
}

void SetupResumeAcceptor::close(
    yarpl::Reference<FrameTransport> tport,
    folly::exception_wrapper e) {
  DCHECK(eventBase_->isInEventBaseThread());

  // This method always gets called with a FrameTransport::onNext() stack frame
  // above it.  Closing the transport too early will destroy it and we'll unwind
  // back up and try to access it.
  eventBase_->runInEventBaseThread(
      [ this, transport = std::move(tport), ew = std::move(e) ]() mutable {
        if (ew) {
          transport->closeWithError(std::move(ew));
        } else {
          transport->close();
        }
        connections_.erase(transport);
      });
}

void SetupResumeAcceptor::remove(
    const yarpl::Reference<FrameTransport>& transport) {
  transport->setFrameProcessor(nullptr);
  connections_.erase(transport);
}

folly::Future<folly::Unit> SetupResumeAcceptor::close() {
  if (eventBase_->isInEventBaseThread()) {
    closeAll();
    return folly::makeFuture();
  }
  return folly::via(eventBase_, [this] { closeAll(); });
}

void SetupResumeAcceptor::closeAll() {
  DCHECK(eventBase_->isInEventBaseThread());

  closed_ = true;

  for (auto& connection : connections_) {
    connection->closeWithError(error("SetupResumeAcceptor is shutting down"));
  }
}
}
