#include <stddef.h>

#include "src/ReactiveStreamsCompat.h"
#include "src/Payload.h"
#include "src/mixins/IntrusiveDeleter.h"
#include "src/mixins/MemoryMixin.h"

using namespace reactivesocket;

class FooSubscriber : public IntrusiveDeleter, public Subscriber<Payload> {
public:
  ~FooSubscriber() override = default;
  void onSubscribe(Subscription&) override {}
  void onNext(Payload) override {}
  void onComplete() override {}
  void onError(folly::exception_wrapper) override {}
};

int main(int argc, char** argv) {
  auto& m = createManagedInstance<FooSubscriber>();
  m.onNext(Payload("asdf"));
  return 0;
}
