#include <iostream>
#include <folly/Memory.h>
#include <folly/io/IOBufQueue.h>
#include "CancelSubscriber.h"

namespace reactivesocket {
    void CancelSubscriber::onSubscribe(Subscription &subscription) {
      subscription.cancel();
    }

    void CancelSubscriber::onNext(Payload element) {
    }

    void CancelSubscriber::onComplete() {
    }

    void CancelSubscriber::onError(folly::exception_wrapper ex) {
    }
}
