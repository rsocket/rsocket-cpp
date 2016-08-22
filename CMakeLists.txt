cmake_minimum_required(VERSION 3.2)
project(ReactiveSocket)

# Cmake modules.
set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/")

# Generate compilation database for use by YouCompleteMe.
set(CMAKE_EXPORT_COMPILE_COMMANDS 1)

# make sure to bail on in-source builds for cleanliness
if (${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR})
  message(FATAL_ERROR "In-source builds not allowed. Please make a new directory (called a build directory)"
          "and run CMake from there. You may need to remove CMakeCache.txt.")
endif()

# default built type is Release
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the type of build" FORCE)
endif(NOT CMAKE_BUILD_TYPE)

# default is to use ReactiveStreams location from github directly
if(NOT REACTIVE_STREAMS_GIT_URL)
  set(REACTIVE_STREAMS_GIT_URL "https://github.com/ReactiveSocket/reactive-streams-cpp.git" CACHE STRING "Location of the ReactiveStreams C++ git repo" FORCE)
endif(NOT REACTIVE_STREAMS_GIT_URL)

enable_testing()

include(ExternalProject)
include(CTest)

# gmock
ExternalProject_Add(
  gmock
  URL ${CMAKE_CURRENT_SOURCE_DIR}/googletest-master-20160722.zip
  INSTALL_COMMAND ""
)

ExternalProject_Get_Property(gmock source_dir)
set(GMOCK_SOURCE_DIR ${source_dir})
ExternalProject_Get_Property(gmock binary_dir)
set(GMOCK_BINARY_DIR ${binary_dir})

set(GMOCK_LIBS
  ${GMOCK_BINARY_DIR}/${CMAKE_CFG_INTDIR}/googlemock/${CMAKE_STATIC_LIBRARY_PREFIX}gmock${CMAKE_STATIC_LIBRARY_SUFFIX}
  ${GMOCK_BINARY_DIR}/${CMAKE_CFG_INTDIR}/googlemock/${CMAKE_STATIC_LIBRARY_PREFIX}gmock_main${CMAKE_STATIC_LIBRARY_SUFFIX}
  )

# ReactiveStreams C++
ExternalProject_Add(
  ReactiveStreams
  GIT_REPOSITORY ${REACTIVE_STREAMS_GIT_URL}
  CMAKE_ARGS "-DCMAKE_BUILD_TYPE=Release" "-DCMAKE_INSTALL_PREFIX=${CMAKE_CURRENT_BINARY_DIR}/reactivestreams"
)

find_package(Threads)

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

include_directories(${CMAKE_CURRENT_BINARY_DIR}/reactivestreams/include)
include_directories(${GMOCK_SOURCE_DIR}/googlemock/include)
include_directories(${GMOCK_SOURCE_DIR}/googletest/include)

add_library(
  ReactiveSocket
  src/AbstractStreamAutomaton.cpp
  src/AbstractStreamAutomaton.h
  src/automata/ChannelRequester.cpp
  src/automata/ChannelRequester.h
  src/automata/ChannelResponder.cpp
  src/automata/ChannelResponder.h
  src/automata/StreamSubscriptionRequesterBase.cpp
  src/automata/StreamSubscriptionRequesterBase.h
  src/automata/StreamSubscriptionResponderBase.cpp
  src/automata/StreamSubscriptionResponderBase.h
  src/automata/StreamRequester.cpp
  src/automata/StreamRequester.h
  src/automata/StreamResponder.cpp
  src/automata/StreamResponder.h
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
  src/NullRequestHandler.cpp
  src/NullRequestHandler.h
  src/Payload.cpp
  src/Payload.h
  src/ReactiveSocket.cpp
  src/ReactiveSocket.h
  src/ReactiveStreamsCompat.h
  src/RequestHandler.h
  src/tcp/TcpDuplexConnection.cpp
  src/tcp/TcpDuplexConnection.h
  src/Stats.h
  src/Stats.cpp
  src/folly/FollyKeepaliveTimer.cpp
  src/folly/FollyKeepaliveTimer.h)

target_link_libraries(
  ReactiveSocket
  ${FOLLY_LIBRARIES}
  glog)

add_dependencies(ReactiveSocket ReactiveStreams)

enable_testing()

# EXCLUDE_FROM_ALL so 'make install' doesn't attempt installation of googletest
#add_subdirectory(external/googletest EXCLUDE_FROM_ALL)

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
  test/Test.cpp
  test/MockStats.h)

target_link_libraries(
  tests
  ReactiveSocket
  ${FOLLY_LIBRARIES}
  ${GMOCK_LIBS}
  gflags
  glog
  ${CMAKE_THREAD_LIBS_INIT})

add_dependencies(tests gmock ReactiveSocket)

add_test(NAME ReactiveSocketTests COMMAND tests)

add_executable(
        tcpclient
        test/tcp/TcpClient.cpp
        test/simple/PrintSubscriber.cpp
        test/simple/PrintSubscriber.h
        src/ReactiveSocket.cpp
        src/ReactiveSocket.h
        test/simple/StatsPrinter.cpp
        test/simple/StatsPrinter.h)

target_link_libraries(
        tcpclient
        ReactiveSocket
        ${FOLLY_LIBRARIES}
        gflags
        ${GMOCK_LIBS}
        glog
        ${CMAKE_THREAD_LIBS_INIT})

add_dependencies(tcpclient gmock)

add_executable(
        tcpserver
        test/tcp/TcpServer.cpp
        test/simple/PrintSubscriber.cpp
        test/simple/PrintSubscriber.h
        test/simple/StatsPrinter.cpp
        test/simple/StatsPrinter.h)

target_link_libraries(
        tcpserver
        ReactiveSocket
        ${FOLLY_LIBRARIES}
        gflags
        ${GMOCK_LIBS}
        glog
        ${CMAKE_THREAD_LIBS_INIT})

add_dependencies(tcpserver gmock)

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
        gflags
        glog)

add_dependencies(tckclient gmock)

# EOF
