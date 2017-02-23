#include "yarpl/Observable_Subscription.h"

namespace yarpl {
namespace observable {

std::unique_ptr<Subscription>
Subscription::create(std::function<void()> onCancel) {
  return std::unique_ptr<Subscription>(new Subscription(onCancel));
}
std::unique_ptr<Subscription>
Subscription::create(std::atomic_bool &cancelled) {
    return create([&cancelled]() { cancelled = true; });
}

std::unique_ptr<Subscription> Subscription::create() {
  return create([]() {});
}

void Subscription::cancel() {
  bool expected = false;
  // mark cancelled 'true' and only if successful invoke 'onCancel()'
  if (cancelled.compare_exchange_strong(expected, true)) {
    onCancel();
  }
}

bool Subscription::isCanceled() { return cancelled; }
}
}