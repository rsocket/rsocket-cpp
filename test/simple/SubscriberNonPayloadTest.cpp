// Copyright 2004-present Facebook. All Rights Reserved.

#include <stddef.h>

#include "src/Payload.h"
#include "src/ReactiveStreamsCompat.h"
#include "src/mixins/IntrusiveDeleter.h"
#include "src/mixins/MemoryMixin.h"

using namespace reactivesocket;

class Foo {
 public:
  explicit Foo(const std::string&) {}
};

class FooSubscriber : public IntrusiveDeleter, public Subscriber<Foo> {
 public:
  ~FooSubscriber() override = default;
  void onSubscribe(Subscription&) override {}
  void onNext(Foo) override {}
  void onComplete() override {}
  void onError(folly::exception_wrapper) override {}
};

int main(int argc, char** argv) {
  auto& m = createManagedInstance<FooSubscriber, Foo>();
  m.onNext(Foo("asdf"));
  return 0;
}
