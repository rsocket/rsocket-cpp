// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once


#include <reactivesocket-cpp/src/ReactiveStreamsCompat.h>
#include <reactivesocket-cpp/src/mixins/IntrusiveDeleter.h>

namespace reactivesocket {
    class CancelSubscription
        : public virtual IntrusiveDeleter,
          public Subscription {
    public:
        ~CancelSubscription() {
        }

        // Subscription methods
        void request(size_t n) override;

        void cancel() override;
    };
}
