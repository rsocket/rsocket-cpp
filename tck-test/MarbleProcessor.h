// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <map>
#include "rsocket/Payload.h"
#include "yarpl/Flowable.h"
#include "yarpl/Single.h"

namespace rsocket {
namespace tck {

class MarbleProcessor {
 public:
  explicit MarbleProcessor(const std::string /* marble */);

  std::tuple<int64_t, bool> run(
      yarpl::flowable::Subscriber<rsocket::Payload>& subscriber,
      int64_t requested);

  void run(yarpl::Reference<yarpl::single::SingleObserver<rsocket::Payload>>
               subscriber);

 private:
  std::string marble_;

  // Stores a mapping from marble character to Payload (data, metadata)
  std::map<std::string, std::pair<std::string, std::string>> argMap_;

  // Keeps an account of how many messages can be sent.  This could be done
  // with Semaphores (AllowanceSemaphore)
  std::atomic<size_t> canSend_{0};

  size_t index_{0};
};

} // tck
} // reactivesocket
