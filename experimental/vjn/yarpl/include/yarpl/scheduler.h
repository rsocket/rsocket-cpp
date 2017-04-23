#pragma once

#include <reactivestreams/type_traits.h>
#include <functional>

namespace yarpl {

class Scheduler {
 public:
  virtual ~Scheduler() = default;
  virtual void schedule(std::function<void()>&& action) = 0;
};

}  // yarpl
