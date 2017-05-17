// Copyright 2004-present Facebook. All Rights Reserved.

#include "RSocketTests.h"

using namespace rsocket;
using namespace rsocket::tests;
using namespace rsocket::tests::client_server;

TEST(RequestResponseTest, StartAndShutdown) {
    makeServer(randPort());
    makeClient(randPort());
}