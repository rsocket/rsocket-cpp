// Copyright 2004-present Facebook. All Rights Reserved.

#include "FireAndForgetRequester.h"

namespace reactivesocket {

void FireAndForgetRequester::request(size_t n) {
	switch (state_) {
		case State::NEW: {
			if (n > 0) {
				Frame_REQUEST_FNF frame(streamId_, FrameFlags_EMPTY, FrameMetadata::empty(), std::move(payload_));
				connection_->onNextFrame(frame);
				state_ = State::COMPLETED;
			} // else -- no-op
			break;
		}
		case State::COMPLETED:
			break;
	}
}

void FireAndForgetRequester::cancel() {
	// no cancellations for fire-and-forget
}
}