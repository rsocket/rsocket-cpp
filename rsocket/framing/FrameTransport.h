#pragma once

#include "yarpl/Refcounted.h"

#include "rsocket/framing/FrameProcessor.h"

namespace rsocket {

// Refer to FrameTransportImpl for documentation on the implementation
class FrameTransport : public virtual yarpl::Refcounted {
 public:
  virtual void setFrameProcessor(std::shared_ptr<FrameProcessor>) = 0;
  virtual void outputFrameOrDrop(std::unique_ptr<folly::IOBuf>) = 0;
  virtual void close() = 0;
  // Just for observation purposes!
  virtual DuplexConnection* getConnection() = 0;
};
}
