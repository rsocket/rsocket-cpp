# Example: stream-hello-world

A basic TCP client/server relationship that streams a sequence of strings using ReactiveSocket `requestStream`.

CMake targets:

- Server: example_stream-hello-world-server
- Client: example_stream-hello-world-client

Server:

```cpp
  // RSocket server accepting on TCP
  auto rs = RSocket::createServer(TcpConnectionAcceptor::create(FLAGS_port));
  // global request handler
  auto handler = std::make_shared<HelloStreamRequestHandler>();
  // start accepting connections
  rs->start([handler](auto connectionRequest) { return handler; });
```

Client:

```cpp
  ScopedEventBaseThread eventBaseThread;
  auto rsf = RSocket::createClient(
      TcpConnectionFactory::create(FLAGS_host, FLAGS_port));
  rsf->connect(eventBaseThread)
      .then([](std::shared_ptr<StandardReactiveSocket> rs) {
        rs->requestStream(
            Payload("args-here"), std::make_shared<ExampleSubscriber>(5, 6));
      });
```

Output:

```
TcpConnectionFactory.cpp:56] ConnectionFactory creation => host: localhost port: 9898
RSocketClient.cpp:13] RSocketClient => created
RSocketClient.cpp:18] RSocketClient => start connection with Future
TcpConnectionFactory.cpp:22] ConnectionFactory => starting socket
TcpConnectionFactory.cpp:25] ConnectionFactory => attempting connection to [::1]:9898
TcpConnectionFactory.cpp:30] ConnectionFactory  => DONE connect
TcpConnectionFactory.cpp:35] ConnectionFactory => socketCallback => Success
RSocketClient.cpp:26] RSocketClient => onConnect received DuplexConnection
ExampleSubscriber.cpp:18] ExampleSubscriber 0x7f9cd1f00eb8 created with =>   Initial Request: 5  Threshold for re-request: 3  Num to Take: 6
ExampleSubscriber.cpp:26] ExampleSubscriber 0x7f9cd1f00eb8 onSubscribe
ExampleSubscriber.cpp:33] ExampleSubscriber 0x7f9cd1f00eb8 onNext as string: Hello Bob 0!
ExampleSubscriber.cpp:33] ExampleSubscriber 0x7f9cd1f00eb8 onNext as string: Hello Bob 1!
ExampleSubscriber.cpp:38] ExampleSubscriber 0x7f9cd1f00eb8 requesting 2 more items
ExampleSubscriber.cpp:33] ExampleSubscriber 0x7f9cd1f00eb8 onNext as string: Hello Bob 2!
ExampleSubscriber.cpp:33] ExampleSubscriber 0x7f9cd1f00eb8 onNext as string: Hello Bob 3!
ExampleSubscriber.cpp:38] ExampleSubscriber 0x7f9cd1f00eb8 requesting 2 more items
ExampleSubscriber.cpp:33] ExampleSubscriber 0x7f9cd1f00eb8 onNext as string: Hello Bob 4!
ExampleSubscriber.cpp:33] ExampleSubscriber 0x7f9cd1f00eb8 onNext as string: Hello Bob 5!
ExampleSubscriber.cpp:38] ExampleSubscriber 0x7f9cd1f00eb8 requesting 2 more items
ExampleSubscriber.cpp:44] ExampleSubscriber 0x7f9cd1f00eb8 cancelling after receiving 6 items.
ExampleSubscriber.cpp:51] ExampleSubscriber 0x7f9cd1f00eb8 onComplete
ExampleSubscriber.cpp:10] ExampleSubscriber destroy 0x7f9cd1f00eb8
```