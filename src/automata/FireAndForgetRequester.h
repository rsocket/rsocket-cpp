// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <memory>

#include "src/Payload.h"

#include "src/ReactiveStreamsCompat.h"
#include "src/mixins/ConsumerMixin.h"
#include "src/mixins/ExecutorMixin.h"
#include "src/mixins/LoggingMixin.h"
#include "src/mixins/MemoryMixin.h"
#include "src/mixins/MixinTerminator.h"
#include "src/mixins/PublisherMixin.h"
#include "src/mixins/SinkIfMixin.h"
#include "src/mixins/SourceIfMixin.h"
#include "src/mixins/StreamIfMixin.h"

namespace reactivesocket {

/// Implementation of an automaton that represents a Fire-and-Forget requester.
/// Note that it's simpler than other automata due to the inherent throwaway
/// simplicity of Fire-and-Forget semantics. Stream IDs are not tracked.
///
/// TODO: we need to add logging similar to other *Requester automata.
class FireAndForgetRequester: public Subscription {
public:
	FireAndForgetRequester(
		std::shared_ptr<ConnectionAutomaton> connection,
		StreamId streamId,
	  Payload payload,
		Subscriber<Payload>& responseSink
	)
	: connection_(connection), streamId_(streamId), payload_(std::move(payload)) {};

	/// @{
	void request(size_t n) override;

	void cancel() override;
	/// @}
private:

	/// State of the Subscription requester.
	enum class State : uint8_t {
		NEW,
		COMPLETED
	} state_{State::NEW};

	/// A partially-owning pointer to the connection to send the Fire-and-Forget frame.
	std::shared_ptr<ConnectionAutomaton> connection_;
	/// An ID of the stream within the connection
	const StreamId streamId_;
	Payload payload_;

};
}