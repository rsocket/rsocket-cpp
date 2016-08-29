// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <cstdint>
#include <memory>
#include <iostream>

#include "Frame.h"

namespace reactivesocket {

class ConnectionAutomaton;

class ResumeCache {
public:
    using position_t = ResumePosition;
    using RetransmitFilter = std::function<bool()>;

    ResumeCache(const std::int64_t length = 0) :
        position_(0)
    {
        // TODO(tmont): create cache of specified length
    }

    ResumeCache(const ResumeCache& cache) :
        position_(cache.position_)
    {
    }

    ResumeCache(ResumeCache&& cache) :
        position_(cache.position_)
    {
    }

    ResumeCache& operator=(const ResumeCache& cache)
    {
        position_ = cache.position_;
        return *this;
    }

    ResumeCache& operator=(ResumeCache&& cache)
    {
        position_ = cache.position_;
        return *this;
    }

    void trackAndCacheSentFrame(const folly::IOBuf &serializedFrame)
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
                position_ += serializedFrame.computeChainDataLength();
                break;

            default:
                break;
        }
    }

    bool isPositionAvailable(position_t position)
    {
        // TODO(tmont): until caching is integrated, we only allow idle resumption
        return (position == position_);
    }

    position_t position()
    {
        return position_;
    }

    bool retransmitFromPosition(
        position_t initialPosition, ConnectionAutomaton &connection, const RetransmitFilter &filter = [](){ return false; });

private:
    position_t position_;
};

}
