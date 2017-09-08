// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include "rsocket/RSocketClient.h"
#include "rsocket/RSocketServer.h"

#include <folly/Optional.h>
#include <folly/io/async/ScopedEventBaseThread.h>

#include <deque>
#include <vector>

namespace rsocket {

/// Benchmarks fixture object that contains a server, along with a list of
/// clients and their worker threads.
///
/// Uses TCP as the transport.
struct Fixture {
  struct Options {
    /// Number of threads the server will run.
    size_t serverThreads{8};

    /// Number of clients to run.
    size_t clients{8};

    /// Number of worker threads driving the clients.  A default value means to
    /// use one thread per client.
    folly::Optional<size_t> clientThreads;
  };

  Fixture(Options, std::shared_ptr<RSocketResponder>);

  // State is public, have at it.

  std::unique_ptr<RSocketServer> server;
  std::deque<std::unique_ptr<folly::ScopedEventBaseThread>> workers;
  std::vector<std::shared_ptr<RSocketClient>> clients;
  const Options options;
};
}
