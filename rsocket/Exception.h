// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <stdexcept>

// Thrown when an ERROR frame with CONNECTION_ERROR is received during
// resuming operation.
// TODO: in this case we should get the DuplexConnection back to
// create a new instance of RS with it
class ResumptionException : public std::runtime_error {
  using std::runtime_error::runtime_error;
};

// Thrown when the resume operation was interrupted due to network
// the application code may try to resume again.
class ConnectionException : public std::runtime_error {
  using std::runtime_error::runtime_error;
};


