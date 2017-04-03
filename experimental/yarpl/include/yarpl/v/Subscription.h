#pragma once

#include <boost/intrusive_ptr.hpp>
#include <boost/smart_ptr/intrusive_ref_counter.hpp>

#include "reactivestreams/ReactiveStreams.h"

namespace yarpl {

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wweak-vtables"

class Subscription : public reactivestreams_yarpl::Subscription,
    public boost::intrusive_ref_counter<Subscription> {
public:
  using Handle = boost::intrusive_ptr<Subscription>;
  virtual ~Subscription() = default;

  virtual void request(int64_t n) = 0;
  virtual void cancel() = 0;

protected:
  Subscription() : reference_(this) {}

  // Drop the reference we're holding on the subscription (handle).
  void release() {
    reference_.reset();
  }

private:
  // We expect to be heap-allocated; until this subscription finishes
  // (is canceled; completes; error's out), hold a reference so we are
  // not deallocated (by the subscriber).
  Handle reference_{this};
};

#pragma clang diagnostic pop

}  // yarpl
