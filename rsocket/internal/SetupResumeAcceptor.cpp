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
}

/// Subscriber that owns a connection, sets itself as that connection's input,
/// and reads out a single frame before cancelling.
class OneFrameSubscriber : public DuplexConnection::Subscriber {
 public:
  OneFrameSubscriber(
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
  }

  void setInput() {
    connection_->setInput(ref_from_this(this));
  }

  void onSubscribe(yarpl::Reference<yarpl::flowable::Subscription> sub) override {
    DuplexConnection::Subscriber::onSubscribe(sub);
    sub->request(std::numeric_limits<int32_t>::max());
  }

  void onNext(std::unique_ptr<folly::IOBuf> buf) override {
    DCHECK(connection_) << "OneFrameSubscriber received more than one frame";

    auto self = ref_from_this(this);
    acceptor_.remove(self);

    if (auto sub = DuplexConnection::Subscriber::subscription()) {
      sub->cancel();
    }

    acceptor_.processFrame(
        std::move(connection_),
        std::move(buf),
        std::move(onSetup_),
        std::move(onResume_));
  }

  void onComplete() override {
    auto self = ref_from_this(this);
    DuplexConnection::Subscriber::onComplete();
    acceptor_.remove(self);
  }

  void onError(folly::exception_wrapper ew) override {
    auto self = ref_from_this(this);
    DuplexConnection::Subscriber::onError(std::move(ew));
    acceptor_.remove(self);
  }

 private:
  SetupResumeAcceptor& acceptor_;
  std::unique_ptr<DuplexConnection> connection_;
  SetupResumeAcceptor::OnSetup onSetup_;
  SetupResumeAcceptor::OnResume onResume_;
};

SetupResumeAcceptor::SetupResumeAcceptor(
    ProtocolVersion version,
    folly::EventBase* eventBase,
    std::thread::id owner)
    : eventBase_{eventBase}, owner_{owner} {
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
    std::unique_ptr<DuplexConnection> connection,
    std::unique_ptr<folly::IOBuf> buf,
    SetupResumeAcceptor::OnSetup onSetup,
    SetupResumeAcceptor::OnResume onResume) {
  DCHECK(eventBase_->isInEventBaseThread());
  DCHECK(connection);

  if (closed_) {
    std::string msg{"SetupResumeAcceptor is shutting down"};
    closeWithError(std::move(connection), std::move(msg));
    return;
  }

  auto serializer = createSerializer(*buf);
  if (!serializer) {
    closeWithError(std::move(connection), "Unable to detect protocol version");
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
  auto subscriber = yarpl::make_ref<OneFrameSubscriber>(
      *this, std::move(connection), std::move(onSetup), std::move(onResume));
  connections_.insert(subscriber);
  subscriber->setInput();
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

void SetupResumeAcceptor::remove(
    const yarpl::Reference<DuplexConnection::Subscriber>& subscriber) {
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

  std::runtime_error exn{"SetupResumeAcceptor is shutting down"};

  auto connections = std::move(connections_);
  for (auto& connection : connections) {
    connection->onSubscribe(yarpl::flowable::Subscription::empty());
    connection->onError(exn);
  }
}

bool SetupResumeAcceptor::inOwnerThread() const {
  if (owner_ != std::thread::id{}) {
    return owner_ == std::this_thread::get_id();
  }
  return eventBase_->isInEventBaseThread();
}
}
