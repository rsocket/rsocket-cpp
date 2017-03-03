#pragma once

#include <functional>
#include <reactivestreams/type_traits.h>

namespace yarpl {

class Scheduler {
public:
  virtual void schedule(std::function<void()>&& action) = 0;
};

}  // yarpl
