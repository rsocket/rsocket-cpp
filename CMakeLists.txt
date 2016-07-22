cmake_minimum_required(VERSION 3.2)
project(ReactiveSocket)

# Cmake modules.
set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/")

# Generate compilation database for use by YouCompleteMe.
set(CMAKE_EXPORT_COMPILE_COMMANDS 1)

# Common configuration for all build modes.
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wno-unused-parameter")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-omit-frame-pointer -momit-leaf-frame-pointer")

# Configuration for Debug build mode.
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fsanitize=address")

find_package(Folly REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(Boost REQUIRED COMPONENTS system thread)
include_directories(SYSTEM ${FOLLY_INCLUDE_DIR})
include_directories(SYSTEM ${OPENSSL_INCLUDE_DIR})

include_directories(${CMAKE_SOURCE_DIR})

add_library(
  ReactiveSocket
  src/AbstractStreamAutomaton.cpp
  src/AbstractStreamAutomaton.h
  src/automata/ChannelRequester.cpp
  src/automata/ChannelRequester.h
  src/automata/ChannelResponder.cpp
  src/automata/ChannelResponder.h
  src/automata/SubscriptionRequester.cpp
  src/automata/SubscriptionRequester.h
  src/automata/SubscriptionResponder.cpp
  src/automata/SubscriptionResponder.h
  src/ConnectionAutomaton.cpp
  src/ConnectionAutomaton.h
  src/DuplexConnection.h
  src/Frame.cpp
  src/Frame.h
  src/framed/FramedDuplexConnection.cpp
  src/framed/FramedDuplexConnection.h
  src/framed/FramedReader.cpp
  src/framed/FramedReader.h
  src/framed/FramedWriter.cpp
  src/framed/FramedWriter.h
  src/mixins/ConsumerMixin-inl.h
  src/mixins/ConsumerMixin.h
  src/mixins/ExecutorMixin.h
  src/mixins/IntrusiveDeleter.h
  src/mixins/LoggingMixin.h
  src/mixins/MemoryMixin.h
  src/mixins/MixinTerminator.h
  src/mixins/PublisherMixin.h
  src/mixins/README.md
  src/mixins/SinkIfMixin.h
  src/mixins/SourceIfMixin.h
  src/mixins/StreamIfMixin.h
  src/Payload.h
  src/ReactiveSocket.cpp
  src/ReactiveSocket.h
  src/ReactiveStreamsCompat.h
  src/RequestHandler.h
  src/tcp/TcpDuplexConnection.cpp
  src/tcp/TcpDuplexConnection.h)

target_link_libraries(
  ReactiveSocket
  ${FOLLY_LIBRARIES}
  glog
  )

enable_testing()

# EXCLUDE_FROM_ALL so 'make install' doesn't attempt installation of googletest
add_subdirectory(external/googletest EXCLUDE_FROM_ALL)

add_executable(
  tests
  test/framed/FramedReaderTest.cpp
  test/framed/FramedWriterTest.cpp
  test/streams/AllowanceSemaphoreTest.cpp
  test/streams/OwnershipTest.cpp
  test/streams/SmartPointersTest.cpp
  test/FrameTest.cpp
  test/InlineConnection.cpp
  test/InlineConnection.h
  test/InlineConnectionTest.cpp
  test/MockDuplexConnection.h
  test/MockRequestHandler.h
  test/ReactiveSocketTest.cpp
  test/ReactiveStreamsMocksCompat.h
  test/Test.cpp)

target_link_libraries(
  tests
  ReactiveSocket
  ${FOLLY_LIBRARIES}
  gflags
  gmock
  gtest
  glog
  )

add_executable(
        tcpclient
        test/tcp/TcpClient.cpp
        test/simple/PrintSubscriber.cpp
        test/simple/PrintSubscriber.h
        test/simple/CancelSubscriber.cpp
        test/simple/CancelSubscriber.h
        src/ReactiveSocket.cpp
        src/ReactiveSocket.h
        test/simple/NullSubscription.cpp
        test/simple/NullSubscription.h)

target_link_libraries(
        tcpclient
        ReactiveSocket
        ${FOLLY_LIBRARIES}
        gtest
        gflags
        gmock
        gmock_main
        glog
)

add_executable(
        tcpserver
        test/tcp/TcpServer.cpp
        test/simple/PrintSubscriber.cpp
        test/simple/PrintSubscriber.h
        test/simple/NullSubscription.cpp
        test/simple/NullSubscription.h
        test/simple/CancelSubscriber.cpp
        test/simple/CancelSubscriber.h)

target_link_libraries(
        tcpserver
        ReactiveSocket
        ${FOLLY_LIBRARIES}
        gtest
        gflags
        gmock
        gmock_main
        glog
)

add_executable(
        tckclient
        tck-test/client.cpp
        tck-test/TestFileParser.cpp
        tck-test/TestFileParser.h
        tck-test/TestSuite.cpp
        tck-test/TestSuite.h
        tck-test/TestInterpreter.cpp
        tck-test/TestInterpreter.h
        tck-test/TypedCommands.h)

target_link_libraries(
        tckclient
        ${FOLLY_LIBRARIES}
        gtest
        gflags
        glog
)

# EOF
