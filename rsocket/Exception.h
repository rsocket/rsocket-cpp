// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <stdexcept>

// Thrown when an ERROR frame with CONNECTION_ERROR is received during
// resumption.
class ResumptionException : public std::runtime_error {
  using std::runtime_error::runtime_error;
};

// Thrown when the resume operation was interrupted due to network.
// The application may try to resume again.
class ConnectionException : public std::runtime_error {
  using std::runtime_error::runtime_error;
};