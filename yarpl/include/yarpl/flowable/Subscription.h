#pragma once

#include "../Refcounted.h"

namespace yarpl {
namespace flowable {

class Subscription : public virtual Refcounted {
 public:
  virtual ~Subscription() = default;

  virtual void request(int64_t n) = 0;
  virtual void cancel() = 0;
};

} // flowable
} // yarpl
