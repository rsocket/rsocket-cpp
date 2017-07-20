// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <stdexcept>

namespace rsocket {

class RSocketException : public std::runtime_error {
  using std::runtime_error::runtime_error;
};

// Thrown when an ERROR frame with CONNECTION_ERROR or REJECTED_RESUME is
// received during resumption.
class ResumptionException : public RSocketException {
  using RSocketException::RSocketException;
};

// Thrown when the resume operation was interrupted due to network.
// The application may try to resume again.
class ConnectionException : public RSocketException {
  using RSocketException::RSocketException;
};
}
