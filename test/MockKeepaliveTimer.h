// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <chrono>
#include <memory>

#include <gmock/gmock.h>

#include "src/ConnectionAutomaton.h"
#include "src/ReactiveSocket.h"


namespace reactivesocket {
class MockKeepaliveTimer : public KeepaliveTimer {
public:
    MockKeepaliveTimer() {
        VLOG(2) << "ctor MockKeepaliveTimer " << this;
    }

    ~MockKeepaliveTimer() {
        VLOG(2) << "dtor MockKeepaliveTimer " << this;
    }

    MOCK_METHOD1(start_,
    void(
    const std::shared_ptr <ConnectionAutomaton> connection
    ));
    MOCK_METHOD0(stop_,
    void());

    void start(const std::shared_ptr<ConnectionAutomaton>& connection) override {
        start_(connection);
    }

    void stop() override {
        stop_();
    }

    virtual std::chrono::milliseconds keepaliveTime() override {
        return std::chrono::seconds(30);
    }
};
}
