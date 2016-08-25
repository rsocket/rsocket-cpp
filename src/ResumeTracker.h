// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <cstdint>
#include <memory>

#include "Frame.h"

namespace reactivesocket {

class ResumeTracker {
public:
    using position_t = ResumePosition;

    ResumeTracker() :
        implied_position_(0)
    {
    }

    void trackReceivedFrame(const folly::IOBuf &serializedFrame)
    {
        auto frameType = FrameHeader::peekType(serializedFrame);

        switch (frameType)
        {
            case FrameType::REQUEST_CHANNEL:
            case FrameType::REQUEST_STREAM:
            case FrameType::REQUEST_SUB:
            case FrameType::REQUEST_N:
            case FrameType::CANCEL:
            case FrameType::ERROR:
            case FrameType::RESPONSE:
                // TODO(tmont): this could be expensive, find a better way to determine frame length
                implied_position_ += serializedFrame.computeChainDataLength();
                break;

            default:
                break;
        }
    }

    position_t impliedPosition()
    {
        return implied_position_;
    }

private:
    position_t implied_position_;
};

}
