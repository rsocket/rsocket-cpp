// Copyright 2004-present Facebook. All Rights Reserved.

#include <benchmark/benchmark.h>
#include <thread>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <iostream>
#include <arpa/inet.h>
#include <atomic>
#include <cstring>

#define MAX_MESSAGE_LENGTH (8 * 1024)
#define PORT (35437)

static void BM_Baseline_TCP_Throughput(benchmark::State &state)
{
    std::atomic<bool> accepting{false};
    std::atomic<bool> accepted{false};
    std::atomic<bool> running{true};
    std::uint64_t totalBytesReceived = 0;
    std::size_t msgLength = static_cast<std::size_t>(state.range(0));
    std::size_t recvLength = static_cast<std::size_t>(state.range(1));

    std::thread t(
        [&]()
        {
            int serverSock = socket(AF_INET, SOCK_STREAM, 0);
            int sock = -1;
            struct sockaddr_in addr;
            socklen_t addrlen = sizeof(addr);
            char message[MAX_MESSAGE_LENGTH];

            std::memset(message, 0, sizeof(message));
            std::memset(&addr, 0, sizeof(addr));

            if (serverSock < 0)
            {
                state.SkipWithError("socket acceptor");
                perror("acceptor socket");
                return;
            }

            int enable = 1;
            if (setsockopt(serverSock, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable)) < 0)
            {
                state.SkipWithError("setsockopt SO_REUSEADDR");
                perror("setsocketopt SO_REUSEADDR");
                return;
            }

            addr.sin_family = AF_INET;
            addr.sin_addr.s_addr = htonl(INADDR_ANY);
            addr.sin_port = htons(PORT);
            if (bind(serverSock, reinterpret_cast<struct sockaddr *>(&addr), addrlen) < 0)
            {
                state.SkipWithError("bind");
                perror("bind");
                return;
            }

            if (listen(serverSock, 1) < 0)
            {
                state.SkipWithError("listen");
                perror("listen");
                return;
            }

            accepting.store(true);

            if ((sock = accept(serverSock, reinterpret_cast<struct sockaddr *>(&addr), &addrlen)) < 0)
            {
                state.SkipWithError("accept");
                perror("accept");
                return;
            }

            accepted.store(true);

            while (running)
            {
                if (send(sock, message, msgLength, 0) != static_cast<ssize_t>(msgLength))
                {
                    state.SkipWithError("send too short");
                    perror("send");
                    return;
                }
            }

            close(sock);
            close(serverSock);
        });

    while (!accepting)
    {
        std::this_thread::yield();
    }

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr;
    socklen_t addrlen = sizeof(addr);
    char message[MAX_MESSAGE_LENGTH];

    std::memset(message, 0, sizeof(message));
    std::memset(&addr, 0, sizeof(addr));

    if (sock < 0)
    {
        state.SkipWithError("socket connector");
        perror("connector socket");
        return;
    }

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    addr.sin_port = htons(PORT);
    if (connect(sock, reinterpret_cast<struct sockaddr *>(&addr), addrlen) < 0)
    {
        state.SkipWithError("connect");
        perror("connect");
        return;
    }

    while (!accepted)
    {
        std::this_thread::yield();
    }

    while (state.KeepRunning())
    {
        ssize_t recved = recv(sock, message, recvLength, 0);

        if (recved < 0)
        {
            state.SkipWithError("recv");
            perror("recv");
            return;
        }

        totalBytesReceived += recved;
    }

    running.store(false);

    close(sock);

    state.SetBytesProcessed(totalBytesReceived);
    state.SetItemsProcessed(totalBytesReceived / msgLength);

    t.join();
}

BENCHMARK(BM_Baseline_TCP_Throughput)
    ->Args({40, 1024})->Args({40, 4096})->Args({80, 4096})->Args({4096, 4096});

static void BM_Baseline_TCP_Latency(benchmark::State &state)
{
    std::atomic<bool> accepting{false};
    std::atomic<bool> accepted{false};
    std::atomic<bool> running{true};
    std::uint64_t totalBytesReceived = 0;
    std::uint64_t totalMsgsExchanged = 0;
    std::size_t msgLength = static_cast<std::size_t>(state.range(0));

    std::thread t(
        [&]()
        {
            int serverSock = socket(AF_INET, SOCK_STREAM, 0);
            int sock = -1;
            struct sockaddr_in addr;
            socklen_t addrlen = sizeof(addr);
            char message[MAX_MESSAGE_LENGTH];

            std::memset(message, 0, sizeof(message));
            std::memset(&addr, 0, sizeof(addr));

            if (serverSock < 0)
            {
                state.SkipWithError("socket acceptor");
                perror("acceptor socket");
                return;
            }

            int enable = 1;
#if defined(SO_REUSEADDR)
            if (setsockopt(serverSock, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable)) < 0)
            {
                state.SkipWithError("setsockopt SO_REUSEADDR");
                perror("setsocketopt SO_REUSEADDR");
                return;
            }
#endif
#if defined(SO_REUSEPORT)
            enable = 1;
            if (setsockopt(serverSock, SOL_SOCKET, SO_REUSEPORT, &enable, sizeof(enable)) < 0)
            {
                state.SkipWithError("setsockopt SO_REUSEPORT");
                perror("setsocketopt SO_REUSEPORT");
                return;
            }
#endif

            addr.sin_family = AF_INET;
            addr.sin_addr.s_addr = htonl(INADDR_ANY);
            addr.sin_port = htons(PORT);
            if (bind(serverSock, reinterpret_cast<struct sockaddr *>(&addr), addrlen) < 0)
            {
                state.SkipWithError("bind");
                perror("bind");
                return;
            }

            if (listen(serverSock, 1) < 0)
            {
                state.SkipWithError("listen");
                perror("listen");
                return;
            }

            accepting.store(true);

            if ((sock = accept(serverSock, reinterpret_cast<struct sockaddr *>(&addr), &addrlen)) < 0)
            {
                state.SkipWithError("accept");
                perror("accept");
                return;
            }

            accepted.store(true);

            while (running)
            {
                if (send(sock, message, msgLength, 0) != static_cast<ssize_t>(msgLength))
                {
                    state.SkipWithError("thread send too short");
                    perror("thread send");
                    break;
                }

                ssize_t recved = recv(sock, message, sizeof(message), 0);

                if (recved < 0 && running)  // may end while blocked on recv, so ignore error if that happens
                {
                    state.SkipWithError("thread recv");
                    perror("thread recv");
                    break;
                }
            }

            close(sock);
            close(serverSock);
        });

    while (!accepting)
    {
        std::this_thread::yield();
    }

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr;
    socklen_t addrlen = sizeof(addr);
    char message[MAX_MESSAGE_LENGTH];

    std::memset(message, 0, sizeof(message));
    std::memset(&addr, 0, sizeof(addr));

    if (sock < 0)
    {
        state.SkipWithError("socket connector");
        perror("connector socket");
        return;
    }

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    addr.sin_port = htons(PORT);
    if (connect(sock, reinterpret_cast<struct sockaddr *>(&addr), addrlen) < 0)
    {
        state.SkipWithError("connect");
        perror("connect");
        return;
    }

    while (!accepted)
    {
        std::this_thread::yield();
    }

    while (state.KeepRunning())
    {
        ssize_t recved = recv(sock, message, sizeof(message), 0);

        if (recved < 0)
        {
            state.SkipWithError("main recv");
            perror("main recv");
            break;
        }

        if (send(sock, message, msgLength, 0) != static_cast<ssize_t>(msgLength))
        {
            state.SkipWithError("main send too short");
            perror("main send");
            break;
        }

        totalMsgsExchanged++;
        totalBytesReceived += recved;
    }

    running.store(false);

    close(sock);

    state.SetBytesProcessed(totalBytesReceived);
    state.SetItemsProcessed(totalMsgsExchanged);

    t.join();
}

BENCHMARK(BM_Baseline_TCP_Latency)
    ->Arg(32)->Arg(128)->Arg(4096);

BENCHMARK_MAIN();
