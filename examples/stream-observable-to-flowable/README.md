# Example: stream-observable-to-flowable

Demonstrate a server with a pure push `Observable` converted to a `Flowable` that drops messages when backpressure occurs from the client. 

CMake targets:

- Server: example_stream-observable-to-flowable-server
- Client: example_stream-observable-to-flowable-client

When running the client will get output such as:

```
Event[TopicX]-1!
Event[TopicX]-2!
Event[TopicX]-3!
Event[TopicX]-138613!
Event[TopicX]-138614!
Event[TopicX]-138615!
Event[TopicX]-237589!
Event[TopicX]-237590!
Event[TopicX]-237591!
Event[TopicX]-381804!
```

This is using a flow control batch size of 3, so it grants credits to the server in batches of 3. 

This is seen in how the events are received. It receives 3 sequential items, and then drops everything while processing until the next batch.

The reason this happens in this example is the client is simulating slow processing (100ms sleep on each event) while the server keeps emitting events.

Without the flow control buffers would overrun. Instead, events are dropped on the server. 