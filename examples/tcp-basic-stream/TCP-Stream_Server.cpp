// Copyright 2004-present Facebook. All Rights Reserved.

#include <iostream>
#include <folly/Memory.h>
#include <folly/io/async/AsyncServerSocket.h>
#include "src/framed/FramedDuplexConnection.h"
#include "src/tcp/TcpDuplexConnection.h"
#include "ServerRequestHandler.h"

using namespace ::reactivesocket;
using namespace ::folly;

DEFINE_string(address, "9898", "host:port to listen to");

namespace {

    class SocketCallback : public AsyncServerSocket::AcceptCallback {
    public:
        SocketCallback(EventBase& eventBase)
                : eventBase_(eventBase){};

        virtual ~SocketCallback() = default;

        virtual void connectionAccepted(
                int fd,
                const SocketAddress& clientAddr) noexcept override {
            std::cout << "connectionAccepted" << clientAddr.describe() << std::endl;

            auto socket =
                    folly::AsyncSocket::UniquePtr(new AsyncSocket(&eventBase_, fd));

            std::unique_ptr<DuplexConnection> connection =
                    std::make_unique<TcpDuplexConnection>(
                            std::move(socket), inlineExecutor());
            std::unique_ptr<DuplexConnection> framedConnection =
                    std::make_unique<FramedDuplexConnection>(
                            std::move(connection), inlineExecutor());
            std::unique_ptr<RequestHandler> requestHandler =
                    std::make_unique<ServerRequestHandler>();

            auto rs = StandardReactiveSocket::fromServerConnection(
                    eventBase_,
                    std::move(framedConnection),
                    std::move(requestHandler));

            rs->onClosed([ this, rs = rs.get() ](const folly::exception_wrapper& ex) {
                removeSocket(*rs);
            });

            reactiveSockets_.push_back(std::move(rs));
        }

        void removeSocket(ReactiveSocket& socket) {
            if (!shuttingDown) {
                reactiveSockets_.erase(std::remove_if(
                        reactiveSockets_.begin(),
                        reactiveSockets_.end(),
                        [&socket](std::unique_ptr<StandardReactiveSocket>& vecSocket) {
                            return vecSocket.get() == &socket;
                        }));
            }
        }

        virtual void acceptError(const std::exception& ex) noexcept override {
            std::cout << "acceptError" << ex.what() << std::endl;
        }

        void shutdown() {
            shuttingDown = true;
            reactiveSockets_.clear();
        }

    private:
        std::vector<std::unique_ptr<StandardReactiveSocket>> reactiveSockets_;
        EventBase& eventBase_;
        bool shuttingDown{false};
    };
}

int main(int argc, char* argv[]) {
    FLAGS_logtostderr = true;
    FLAGS_minloglevel = 0;

    google::ParseCommandLineFlags(&argc, &argv, true);
    google::InitGoogleLogging(argv[0]);
    google::InstallFailureSignalHandler();

    EventBase eventBase;
    auto thread = std::thread([&eventBase]() { eventBase.loopForever(); });

    SocketCallback callback(eventBase);

    auto serverSocket = AsyncServerSocket::newSocket(&eventBase);

    eventBase.runInEventBaseThreadAndWait(
            [&callback, &eventBase, &serverSocket]() {
                folly::SocketAddress addr;
                addr.setFromLocalIpPort(FLAGS_address);

                serverSocket->setReusePortEnabled(true);
                serverSocket->bind(addr);
                serverSocket->addAcceptCallback(&callback, &eventBase);
                serverSocket->listen(10);
                serverSocket->startAccepting();

                std::cout << "server listening on ";
                for (auto i : serverSocket->getAddresses())
                    std::cout << i.describe() << ' ';
                std::cout << '\n';
            });

    std::string name;
    std::getline(std::cin, name);

    eventBase.runInEventBaseThreadAndWait([&callback]() { callback.shutdown(); });
    eventBase.terminateLoopSoon();

    thread.join();
}
