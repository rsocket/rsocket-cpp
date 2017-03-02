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
  rs->startAndPark([handler](auto r) { return handler; });
```

Client:

```cpp
auto rsf = RSocket::createClient(TcpConnectionFactory::create(host, port));
auto s = std::make_shared<ExampleSubscriber>(5, 6);
auto rs = rsf->connect().get();
rs->requestStream(Payload("Jane"), s);
```
