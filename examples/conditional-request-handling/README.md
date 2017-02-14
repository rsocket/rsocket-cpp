# Example: conditional-request-subscription

Example server that uses the connection SETUP data (MimeType in this example) to determine which request handler to use. 

CMake targets:

- Server: example_conditional-request-handling-server
- Client: example_conditional-request-handling-client

Server:

```cpp
// RSocket server accepting on TCP
auto rs = RSocket::createServer(TcpConnectionAcceptor::create(FLAGS_port));
// global request handlers
auto textHandler = std::make_shared<TextRequestHandler>();
auto jsonHandler = std::make_shared<JsonRequestHandler>();
// start accepting connections
rs->startAndPark(
  [textHandler, jsonHandler](std::unique_ptr<ConnectionSetupRequest> r)
      -> std::shared_ptr<RequestHandler> {
        if (r->getDataMimeType() == "text/plain") {
          return textHandler;
        } else if (r->getDataMimeType() == "application/json") {
          return jsonHandler;
        } else {
          throw UnsupportedSetupError("Unknown MimeType");
        }
      });
```

Client:

```cpp
// TODO not yet passing in MimeType
```
