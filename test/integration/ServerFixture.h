// Copyright 2004-present Facebook. All Rights Reserved.

#include <gmock/gmock.h>

#include <folly/io/async/AsyncServerSocket.h>

#include "src/temporary_home/NullRequestHandler.h"
#include "test/deprecated/ReactiveSocket.h"
#include "src/temporary_home/ServerConnectionAcceptor.h"
#include "src/temporary_home/SubscriptionBase.h"
#include "src/framing/FramedDuplexConnection.h"
#include "src/transports/tcp/TcpDuplexConnection.h"

// A fixture class to init/destroy a ReactiveSocket server.  This can be used
// to run different types of unittests.  The server's processes the request(n)
// methods and sends n frames.  The frame content is just the frame number.
class ServerFixture : public testing::Test {
 public:
  // ReactiveSocket requires a specific order in which objects need to be
  // created and destroyed.  The constructor and destructor ensure the order
  // is respected.
  ServerFixture();
  ~ServerFixture();

  // The EventBase which runs the server thread.
  folly::EventBase eventBase_;

  // The thread which listens and accepts connections from clients.
  std::thread serverAcceptThread_;

  // The socket on which the server listens to client connections.
  folly::AsyncServerSocket::UniquePtr serverAcceptSocket_;

  // Callback methods for accept actions.
  std::unique_ptr<folly::AsyncServerSocket::AcceptCallback> myAcceptCallback_;

  // Port used by server to listen to new connections
  uint16_t serverListenPort_{0};
};
