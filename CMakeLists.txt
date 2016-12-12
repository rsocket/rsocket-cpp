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
  URL ${CMAKE_CURRENT_SOURCE_DIR}/googletest-release-1.8.0.zip
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

if(APPLE)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14")
else()
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
endif(APPLE)

# Common configuration for all build modes.
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wno-unused-parameter -Werror -Woverloaded-virtual")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-omit-frame-pointer -momit-leaf-frame-pointer")

# Configuration for Debug build mode.
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fsanitize=address")

find_package(Folly REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(Boost REQUIRED COMPONENTS system thread)

# Find glog and gflags libraries specifically
find_path(GLOG_INCLUDE_DIR glog/logging.h)
find_path(GFLAGS_INCLUDE_DIR gflags/gflags.h)

find_library(GLOG_LIBRARY glog)
find_library(GFLAGS_LIBRARY gflags)

message("gflags include_dir <${GFLAGS_INCLUDE_DIR}> lib <${GFLAGS_LIBRARY}>")
message("glog include_dir <${GLOG_INCLUDE_DIR}> lib <${GLOG_LIBRARY}>")

include_directories(SYSTEM ${FOLLY_INCLUDE_DIR})
include_directories(SYSTEM ${OPENSSL_INCLUDE_DIR})

include_directories(SYSTEM ${GFLAGS_INCLUDE_DIR})
include_directories(SYSTEM ${GLOG_INCLUDE_DIR})

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
  src/automata/RequestResponseRequester.cpp
  src/automata/RequestResponseRequester.h
  src/automata/RequestResponseResponder.cpp
  src/automata/RequestResponseResponder.h
  src/automata/StreamRequester.cpp
  src/automata/StreamRequester.h
  src/automata/StreamResponder.h
  src/automata/StreamSubscriptionRequesterBase.cpp
  src/automata/StreamSubscriptionRequesterBase.h
  src/automata/StreamSubscriptionResponderBase.cpp
  src/automata/StreamSubscriptionResponderBase.h
  src/automata/SubscriptionRequester.cpp
  src/automata/SubscriptionRequester.h
  src/automata/SubscriptionResponder.h
  src/Common.cpp
  src/Common.h
  src/concurrent/atomic64_gcc_x86_64.h
  src/concurrent/atomic64.h
  src/concurrent/OneToOneRingBuffer.h
  src/ConnectionAutomaton.cpp
  src/ConnectionAutomaton.h
  src/ConnectionSetupPayload.cpp
  src/ConnectionSetupPayload.h
  src/DuplexConnection.h
  src/EnableSharedFromThis.h
  src/Executor.cpp
  src/folly/FollyKeepaliveTimer.cpp
  src/folly/FollyKeepaliveTimer.h
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
  src/mixins/MixinTerminator.h
  src/mixins/PublisherMixin.h
  src/mixins/StreamIfMixin.h
  src/NullRequestHandler.cpp
  src/NullRequestHandler.h
  src/Payload.cpp
  src/Payload.h
  src/ReactiveSocket.cpp
  src/ReactiveSocket.h
  src/ReactiveSocketSubscriberFactory.h
  src/ReactiveStreamsCompat.h
  src/RequestHandler.cpp
  src/RequestHandler.h
  src/ResumeCache.cpp
  src/ResumeCache.h
  src/ResumeTracker.h
  src/Stats.cpp
  src/Stats.h
  src/StreamState.h
  src/SubscriberBase.h
  src/SubscriptionBase.h
  src/tcp/TcpDuplexConnection.cpp
  src/tcp/TcpDuplexConnection.h
  src/Executor.h)


target_link_libraries(
  ReactiveSocket
  ${FOLLY_LIBRARIES}
  ${GLOG_LIBRARY})

add_dependencies(ReactiveSocket ReactiveStreams)

enable_testing()

# EXCLUDE_FROM_ALL so 'make install' doesn't attempt installation of googletest
#add_subdirectory(external/googletest EXCLUDE_FROM_ALL)

add_executable(
  tests
  test/concurrent/OneToOneRingBufferTest.cpp
  test/ConnectionAutomatonTest.cpp
  test/ExecutorTest.cpp
  test/framed/FramedReaderTest.cpp
  test/framed/FramedWriterTest.cpp
  test/FrameTest.cpp
  test/InlineConnection.cpp
  test/InlineConnection.h
  test/InlineConnectionTest.cpp
  test/MockDuplexConnection.h
  test/MockKeepaliveTimer.h
  test/MockRequestHandler.h
  test/MockStats.h
  test/ReactiveSocketConcurrencyTest.cpp
  test/ReactiveSocketTest.cpp
  test/ReactiveStreamsMocksCompat.h
  test/SubscriberBaseTest.cpp
  test/Test.cpp
  test/folly/FollyKeepaliveTimerTest.cpp)

target_link_libraries(
  tests
  ReactiveSocket
  ${FOLLY_LIBRARIES}
  ${GMOCK_LIBS}
  ${GFLAGS_LIBRARY}
  ${GLOG_LIBRARY}
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
        ${GFLAGS_LIBRARY}
        ${GMOCK_LIBS}
        ${GLOG_LIBRARY}
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
        ${GFLAGS_LIBRARY}
        ${GMOCK_LIBS}
        ${GLOG_LIBRARY}
        ${CMAKE_THREAD_LIBS_INIT})

add_dependencies(tcpserver gmock)

add_executable(
        tckclient
        tck-test/client.cpp
        tck-test/TestFileParser.cpp
        tck-test/TestFileParser.h
        tck-test/TestSubscriber.cpp
        tck-test/TestSubscriber.h
        tck-test/TestSuite.cpp
        tck-test/TestSuite.h
        tck-test/TestInterpreter.cpp
        tck-test/TestInterpreter.h
        tck-test/TypedCommands.h)

target_link_libraries(
        tckclient
        ReactiveSocket
        ${FOLLY_LIBRARIES}
        ${GFLAGS_LIBRARY}
        ${GLOG_LIBRARY}
        ${CMAKE_THREAD_LIBS_INIT})

add_dependencies(tckclient gmock)

add_executable(
        tcpresumeclient
        test/resume/TcpResumeClient.cpp
        test/simple/PrintSubscriber.cpp
        test/simple/PrintSubscriber.h
        src/ReactiveSocket.cpp
        src/ReactiveSocket.h
        test/simple/StatsPrinter.cpp
        test/simple/StatsPrinter.h)

target_link_libraries(
        tcpresumeclient
        ReactiveSocket
        ${FOLLY_LIBRARIES}
        ${GFLAGS_LIBRARY}
        ${GMOCK_LIBS}
        ${GLOG_LIBRARY}
        ${CMAKE_THREAD_LIBS_INIT})

add_dependencies(tcpresumeclient gmock)

add_executable(
        tcpresumeserver
        test/resume/TcpResumeServer.cpp
        test/simple/PrintSubscriber.cpp
        test/simple/PrintSubscriber.h
        test/simple/StatsPrinter.cpp
        test/simple/StatsPrinter.h)

target_link_libraries(
        tcpresumeserver
        ReactiveSocket
        ${FOLLY_LIBRARIES}
        ${GFLAGS_LIBRARY}
        ${GMOCK_LIBS}
        ${GLOG_LIBRARY}
        ${CMAKE_THREAD_LIBS_INIT})

add_dependencies(tcpresumeserver gmock)

# EOF
