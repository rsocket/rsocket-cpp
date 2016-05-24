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
find_package(Boost REQUIRED COMPONENTS system thread)
include_directories(SYSTEM ${FOLLY_INCLUDE_DIR})

include_directories(${CMAKE_SOURCE_DIR})

add_library(
  ReactiveSocket
  reactivesocket-cpp/src/AbstractStreamAutomaton.cpp
  reactivesocket-cpp/src/AbstractStreamAutomaton.h
  reactivesocket-cpp/src/automata/ChannelRequester.cpp
  reactivesocket-cpp/src/automata/ChannelRequester.h
  reactivesocket-cpp/src/automata/ChannelResponder.cpp
  reactivesocket-cpp/src/automata/ChannelResponder.h
  reactivesocket-cpp/src/automata/SubscriptionRequester.cpp
  reactivesocket-cpp/src/automata/SubscriptionRequester.h
  reactivesocket-cpp/src/automata/SubscriptionResponder.cpp
  reactivesocket-cpp/src/automata/SubscriptionResponder.h
  reactivesocket-cpp/src/ConnectionAutomaton.cpp
  reactivesocket-cpp/src/ConnectionAutomaton.h
  reactivesocket-cpp/src/DuplexConnection.h
  reactivesocket-cpp/src/Frame.cpp
  reactivesocket-cpp/src/Frame.h
  reactivesocket-cpp/src/mixins/ConsumerMixin-inl.h
  reactivesocket-cpp/src/mixins/ConsumerMixin.h
  reactivesocket-cpp/src/mixins/ExecutorMixin.h
  reactivesocket-cpp/src/mixins/IntrusiveDeleter.h
  reactivesocket-cpp/src/mixins/LoggingMixin.h
  reactivesocket-cpp/src/mixins/MemoryMixin.h
  reactivesocket-cpp/src/mixins/MixinTerminator.h
  reactivesocket-cpp/src/mixins/PublisherMixin.h
  reactivesocket-cpp/src/mixins/README.md
  reactivesocket-cpp/src/mixins/SinkIfMixin.h
  reactivesocket-cpp/src/mixins/SourceIfMixin.h
  reactivesocket-cpp/src/mixins/StreamIfMixin.h
  reactivesocket-cpp/src/Payload.h
  reactivesocket-cpp/src/ReactiveSocket.cpp
  reactivesocket-cpp/src/ReactiveSocket.h
  reactivesocket-cpp/src/ReactiveStreamsCompat.h
  reactivesocket-cpp/src/RequestHandler.h
  )

target_link_libraries(
  ReactiveSocket
  ${FOLLY_LIBRARIES}
  glog
  )

enable_testing()

add_subdirectory(external/googletest EXCLUDE_FROM_ALL)
# EXCLUDE_FROM_ALL so 'make install' doesn't attempt installation of googletest

add_executable(
  tests
  reactivesocket-cpp/test/streams/AllowanceSemaphoreTest.cpp
  reactivesocket-cpp/test/streams/OwnershipTest.cpp
  reactivesocket-cpp/test/streams/SmartPointersTest.cpp
  reactivesocket-cpp/test/FrameTest.cpp
  reactivesocket-cpp/test/InlineConnection.cpp
  reactivesocket-cpp/test/InlineConnection.h
  reactivesocket-cpp/test/InlineConnectionTest.cpp
  reactivesocket-cpp/test/MockDuplexConnection.h
  reactivesocket-cpp/test/MockRequestHandler.h
  reactivesocket-cpp/test/ReactiveSocketTest.cpp
  reactivesocket-cpp/test/ReactiveStreamsMocksCompat.h
  )

target_link_libraries(
  tests
  ReactiveSocket
  ${FOLLY_LIBRARIES}
  gtest
  gmock
  gmock_main
  glog
  )

# EOF
