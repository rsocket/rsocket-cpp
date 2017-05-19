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

# default built type is Debug
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Choose the type of build" FORCE)
endif(NOT CMAKE_BUILD_TYPE)

# default is to use ReactiveStreams location from github directly
if(NOT REACTIVE_STREAMS_GIT_URL)
  set(REACTIVE_STREAMS_GIT_URL "https://github.com/ReactiveSocket/reactive-streams-cpp.git" CACHE STRING "Location of the ReactiveStreams C++ git repo" FORCE)
endif(NOT REACTIVE_STREAMS_GIT_URL)

option(BUILD_BENCHMARKS "Build benchmarks" OFF)

enable_testing()

# Add a OSS macro.  This is mainly to get gflags working with travis.
add_definitions(-DOSS)

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
  GIT_TAG d2fd61252b51a57a2916ee52fcd54b7f5d563591
  CMAKE_ARGS "-DCMAKE_BUILD_TYPE=Debug" /
    "-DCMAKE_INSTALL_PREFIX=${CMAKE_CURRENT_BINARY_DIR}/reactivestreams" /
    "-DCMAKE_CXX_FLAGS=${CMAKE_CXX_FLAGS}" /
    "-DCMAKE_EXE_LINKER_FLAGS=${CXX_LINKER_FLAGS}"
)

find_package(Threads)

string(TOLOWER "${CMAKE_BUILD_TYPE}" BUILD_TYPE_LOWER)

if(APPLE)
  set(OPENSSL_ROOT_DIR "/usr/local/opt/openssl")
  if("${BUILD_TYPE_LOWER}" MATCHES "debug")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address,integer -fno-sanitize=unsigned-integer-overflow")
  endif()
endif()

if(NOT META_CXX_STD)
  # Defaults to C++14 if not set:
  set(META_CXX_STD 14)
endif()

# Common configuration for all build modes.
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++${META_CXX_STD} -pedantic")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unused-parameter -Woverloaded-virtual")

set(EXTRA_CXX_FLAGS ${EXTRA_CXX_FLAGS} -Werror)

if("${BUILD_TYPE_LOWER}" MATCHES "debug")
  message("debug mode was set")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unreachable-code")
else()
  message("release mode was set")
endif()

if("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang")
  set(TEST_CXX_FLAGS ${TEST_CXX_FLAGS} -Wno-inconsistent-missing-override)
endif()

find_library(DOUBLE-CONVERSION double-conversion)

find_package(Folly REQUIRED)
find_package(OpenSSL REQUIRED)

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

add_subdirectory(yarpl)

add_library(
  ReactiveSocket
  src/RSocket.cpp
  src/RSocket.h
  src/RSocketServer.h
  src/RSocketServer.cpp
  src/RSocketClient.h
  src/RSocketClient.cpp
  src/RSocketRequester.h
  src/RSocketRequester.cpp
  src/RSocketErrors.h
  src/RSocketParameters.cpp
  src/RSocketParameters.h
  src/ConnectionAcceptor.h
  src/ConnectionFactory.h
  src/ConnectionSetupRequest.h
  src/ConnectionSetupRequest.cpp
  src/ConnectionResumeRequest.h
  src/ConnectionResumeRequest.cpp
  src/transports/tcp/TcpConnectionAcceptor.h
  src/transports/tcp/TcpConnectionAcceptor.cpp
  src/transports/tcp/TcpConnectionFactory.h
  src/transports/tcp/TcpConnectionFactory.cpp
  src/RSocketResponder.cpp
  src/RSocketResponder.h
  src/RSocketConnectionHandler.h
  src/RSocketConnectionHandler.cpp
  src/statemachine/ChannelRequester.cpp
  src/statemachine/ChannelRequester.h
  src/statemachine/ChannelResponder.cpp
  src/statemachine/ChannelResponder.h
  src/statemachine/ConsumerBase.cpp
  src/statemachine/ConsumerBase.h
  src/statemachine/PublisherBase.h
  src/statemachine/RequestResponseRequester.cpp
  src/statemachine/RequestResponseRequester.h
  src/statemachine/RequestResponseResponder.cpp
  src/statemachine/RequestResponseResponder.h
  src/statemachine/StreamStateMachineBase.cpp
  src/statemachine/StreamStateMachineBase.h
  src/statemachine/StreamRequester.cpp
  src/statemachine/StreamRequester.h
  src/statemachine/StreamResponder.cpp
  src/statemachine/StreamResponder.h
  src/statemachine/StreamsFactory.cpp
  src/statemachine/StreamsFactory.h
  src/statemachine/StreamsWriter.h
  src/statemachine/StreamState.cpp
  src/statemachine/StreamState.h
  src/internal/ClientResumeStatusCallback.h
  src/internal/Common.cpp
  src/internal/Common.h
  src/statemachine/RSocketStateMachine.cpp
  src/statemachine/RSocketStateMachine.h
  src/DuplexConnection.h
  src/internal/EnableSharedFromThis.h
  src/temporary_home/Executor.cpp
  src/temporary_home/Executor.h
  src/internal/FollyKeepaliveTimer.cpp
  src/internal/FollyKeepaliveTimer.h
  src/framing/Frame.cpp
  src/framing/Frame.h
  src/framing/FramedDuplexConnection.cpp
  src/framing/FramedDuplexConnection.h
  src/framing/FramedReader.cpp
  src/framing/FramedReader.h
  src/framing/FramedWriter.cpp
  src/framing/FramedWriter.h
  src/framing/FrameProcessor.h
  src/framing/FrameSerializer.cpp
  src/framing/FrameSerializer.h
  src/framing/FrameTransport.cpp
  src/framing/FrameTransport.h
  src/Payload.cpp
  src/Payload.h
  src/internal/ReactiveStreamsCompat.h
  src/internal/ResumeCache.cpp
  src/internal/ResumeCache.h
  src/temporary_home/ServerConnectionAcceptor.cpp
  src/temporary_home/ServerConnectionAcceptor.h
  src/RSocketStats.cpp
  src/RSocketStats.h
  src/temporary_home/SubscriberBase.h
  src/temporary_home/SubscriptionBase.h
  src/transports/tcp/TcpDuplexConnection.cpp
  src/transports/tcp/TcpDuplexConnection.h
  src/framing/FrameSerializer_v0.cpp
  src/framing/FrameSerializer_v0.h
  src/framing/FrameSerializer_v0_1.cpp
  src/framing/FrameSerializer_v0_1.h
  src/framing/FrameSerializer_v1_0.cpp
  src/framing/FrameSerializer_v1_0.h
  src/internal/ScheduledSubscription.cpp
  src/internal/ScheduledSubscription.h
  src/internal/ScheduledSubscriber.h
  src/internal/ScheduledRSocketResponder.cpp
  src/internal/ScheduledRSocketResponder.h
  src/internal/ScheduledSingleObserver.h
  src/internal/ScheduledSingleSubscription.cpp
  src/internal/ScheduledSingleSubscription.h)

target_include_directories(ReactiveSocket PUBLIC "${PROJECT_SOURCE_DIR}/yarpl/include")
target_include_directories(ReactiveSocket PUBLIC "${PROJECT_SOURCE_DIR}/yarpl/src")

target_link_libraries(
  ReactiveSocket
  ${FOLLY_LIBRARIES}
  ${GFLAGS_LIBRARY}
  ${GLOG_LIBRARY})

add_dependencies(ReactiveSocket ReactiveStreams yarpl)

target_compile_options(
  ReactiveSocket
  PRIVATE ${EXTRA_CXX_FLAGS})

enable_testing()

# EXCLUDE_FROM_ALL so 'make install' doesn't attempt installation of googletest
#add_subdirectory(external/googletest EXCLUDE_FROM_ALL)

add_executable(
  tests
  test/Test.cpp
  test/RSocketClientServerTest.cpp
  test/PayloadTest.cpp
  test/handlers/HelloStreamRequestHandler.h
  test/handlers/HelloStreamRequestHandler.cpp
  test/framing/FrameTest.cpp
  test/test_utils/MockKeepaliveTimer.h
  test/test_utils/MockRequestHandler.h
  test/test_utils/MockStats.h
  test/internal/FollyKeepaliveTimerTest.cpp
  test/internal/AllowanceSemaphoreTest.cpp
  test/RSocketTests.h
  test/RequestResponseTest.cpp
  test/RequestStreamTest.cpp
)

target_link_libraries(
  tests
  ReactiveSocket
  yarpl
  ${FOLLY_LIBRARIES}
  ${GMOCK_LIBS}
  ${GFLAGS_LIBRARY}
  ${GLOG_LIBRARY}
  ${CMAKE_THREAD_LIBS_INIT})

target_compile_options(
  tests
  PRIVATE ${TEST_CXX_FLAGS})

add_dependencies(tests gmock ReactiveSocket)

add_test(NAME ReactiveSocketTests COMMAND tests)

#add_executable(
#  tckclient
#  tck-test/client.cpp
#  tck-test/TestFileParser.cpp
#  tck-test/TestFileParser.h
#  tck-test/TestSubscriber.cpp
#  tck-test/TestSubscriber.h
#  tck-test/TestSuite.cpp
#  tck-test/TestSuite.h
#  tck-test/TestInterpreter.cpp
#  tck-test/TestInterpreter.h
#  tck-test/TypedCommands.h
#  test/deprecated/ReactiveSocket.cpp
#  test/deprecated/ReactiveSocket.h
#)
#
#target_link_libraries(
#  tckclient
#  ReactiveSocket
#  yarpl
#  ${FOLLY_LIBRARIES}
#  ${GFLAGS_LIBRARY}
#  ${GLOG_LIBRARY}
#  ${CMAKE_THREAD_LIBS_INIT})
#
#add_executable(
#  tckserver
#  tck-test/server.cpp
#  tck-test/MarbleProcessor.cpp
#  tck-test/MarbleProcessor.h
#  test/test_utils/StatsPrinter.cpp
#  test/test_utils/StatsPrinter.h
#  test/deprecated/ReactiveSocket.cpp
#  test/deprecated/ReactiveSocket.h
#)
#
#target_link_libraries(
#  tckserver
#  ReactiveSocket
#  yarpl
#  ${FOLLY_LIBRARIES}
#  ${GFLAGS_LIBRARY}
#  ${GMOCK_LIBS}
#  ${GLOG_LIBRARY}
#  ${DOUBLE-CONVERSION}
#  ${CMAKE_THREAD_LIBS_INIT})

########################################
# Examples
########################################

add_library(
  reactivesocket_examples_util
  examples/util/ExampleSubscriber.cpp
  examples/util/ExampleSubscriber.h
)

target_link_libraries(
  reactivesocket_examples_util
  yarpl
  ReactiveSocket
  ${FOLLY_LIBRARIES}
  ${GFLAGS_LIBRARY}
  ${GLOG_LIBRARY}
  ${CMAKE_THREAD_LIBS_INIT}
)

# request-response-hello-world

add_executable(
  example_request-response-hello-world-server
  examples/request-response-hello-world/RequestResponseHelloWorld_Server.cpp
)

target_link_libraries(
  example_request-response-hello-world-server
  ReactiveSocket
  reactivesocket_examples_util
  yarpl
  ${FOLLY_LIBRARIES}
  ${GFLAGS_LIBRARY}
  ${GLOG_LIBRARY}
  ${CMAKE_THREAD_LIBS_INIT})

add_executable(
  example_request-response-hello-world-client
  examples/request-response-hello-world/RequestResponseHelloWorld_Client.cpp
)

target_link_libraries(
  example_request-response-hello-world-client
  ReactiveSocket
  reactivesocket_examples_util
  yarpl
  ${FOLLY_LIBRARIES}
  ${GFLAGS_LIBRARY}
  ${GLOG_LIBRARY}
  ${CMAKE_THREAD_LIBS_INIT})

# fire-and-forget-hello-world

add_executable(
  example_fire-and-forget-hello-world-server
  examples/fire-and-forget-hello-world/FireAndForgetHelloWorld_Server.cpp
)

target_link_libraries(
  example_fire-and-forget-hello-world-server
  ReactiveSocket
  reactivesocket_examples_util
  yarpl
  ${FOLLY_LIBRARIES}
  ${GFLAGS_LIBRARY}
  ${GLOG_LIBRARY}
  ${CMAKE_THREAD_LIBS_INIT})

add_executable(
  example_fire-and-forget-hello-world-client
  examples/fire-and-forget-hello-world/FireAndForgetHelloWorld_Client.cpp
)

target_link_libraries(
  example_fire-and-forget-hello-world-client
  ReactiveSocket
  reactivesocket_examples_util
  yarpl
  ${FOLLY_LIBRARIES}
  ${GFLAGS_LIBRARY}
  ${GLOG_LIBRARY}
  ${CMAKE_THREAD_LIBS_INIT})


# stream-hello-world

add_executable(
  example_stream-hello-world-server
  examples/stream-hello-world/StreamHelloWorld_Server.cpp
)

target_link_libraries(
  example_stream-hello-world-server
  ReactiveSocket
  reactivesocket_examples_util
  yarpl
  ${FOLLY_LIBRARIES}
  ${GFLAGS_LIBRARY}
  ${GLOG_LIBRARY}
  ${CMAKE_THREAD_LIBS_INIT})

add_executable(
  example_stream-hello-world-client
  examples/stream-hello-world/StreamHelloWorld_Client.cpp
)

target_link_libraries(
  example_stream-hello-world-client
  ReactiveSocket
  reactivesocket_examples_util
  yarpl
  ${FOLLY_LIBRARIES}
  ${GFLAGS_LIBRARY}
  ${GLOG_LIBRARY}
  ${CMAKE_THREAD_LIBS_INIT})


# channel-hello-world

add_executable(
  example_channel-hello-world-server
  examples/channel-hello-world/ChannelHelloWorld_Server.cpp
)

target_link_libraries(
  example_channel-hello-world-server
  ReactiveSocket
  reactivesocket_examples_util
  yarpl
  ${FOLLY_LIBRARIES}
  ${GFLAGS_LIBRARY}
  ${GLOG_LIBRARY}
  ${CMAKE_THREAD_LIBS_INIT})

add_executable(
  example_channel-hello-world-client
  examples/channel-hello-world/ChannelHelloWorld_Client.cpp
)

target_link_libraries(
  example_channel-hello-world-client
  ReactiveSocket
  reactivesocket_examples_util
  yarpl
  ${FOLLY_LIBRARIES}
  ${GFLAGS_LIBRARY}
  ${GLOG_LIBRARY}
  ${CMAKE_THREAD_LIBS_INIT})

# stream-observable-to-flowable

add_executable(
  example_observable-to-flowable-server
  examples/stream-observable-to-flowable/StreamObservableToFlowable_Server.cpp
)

target_link_libraries(
  example_observable-to-flowable-server
  ReactiveSocket
  reactivesocket_examples_util
  yarpl
  ${FOLLY_LIBRARIES}
  ${GFLAGS_LIBRARY}
  ${GLOG_LIBRARY}
  ${CMAKE_THREAD_LIBS_INIT})

add_executable(
  example_observable-to-flowable-client
  examples/stream-observable-to-flowable/StreamObservableToFlowable_Client.cpp
)

target_link_libraries(
  example_observable-to-flowable-client
  ReactiveSocket
  reactivesocket_examples_util
  yarpl
  ${FOLLY_LIBRARIES}
  ${GFLAGS_LIBRARY}
  ${GLOG_LIBRARY}
  ${CMAKE_THREAD_LIBS_INIT})

# conditional-request-handling

add_executable(
  example_conditional-request-handling-server
  examples/conditional-request-handling/ConditionalRequestHandling_Server.cpp
  examples/conditional-request-handling/TextRequestHandler.h
  examples/conditional-request-handling/TextRequestHandler.cpp
  examples/conditional-request-handling/JsonRequestHandler.cpp
  examples/conditional-request-handling/JsonRequestHandler.h
)

target_link_libraries(
  example_conditional-request-handling-server
  ReactiveSocket
  reactivesocket_examples_util
  yarpl
  ${FOLLY_LIBRARIES}
  ${GFLAGS_LIBRARY}
  ${GLOG_LIBRARY}
  ${CMAKE_THREAD_LIBS_INIT})

add_executable(
  example_conditional-request-handling-client
  examples/conditional-request-handling/ConditionalRequestHandling_Client.cpp
)

target_link_libraries(
  example_conditional-request-handling-client
  ReactiveSocket
  reactivesocket_examples_util
  yarpl
  ${FOLLY_LIBRARIES}
  ${GFLAGS_LIBRARY}
  ${GLOG_LIBRARY}
  ${CMAKE_THREAD_LIBS_INIT})


########################################
# End Examples
########################################

#if(BUILD_BENCHMARKS)
  add_subdirectory(benchmarks)
#endif(BUILD_BENCHMARKS)

# EOF
