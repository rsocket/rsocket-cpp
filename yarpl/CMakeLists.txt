cmake_minimum_required (VERSION 3.2)

# To debug the project, set the build type.
set(CMAKE_BUILD_TYPE Debug)

project (yarpl)

# CMake Config

add_definitions(-std=c++14)

# Generate compilation database
set(CMAKE_EXPORT_COMPILE_COMMANDS 1)

# Common configuration for all build modes.
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wno-unused-parameter")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-weak-vtables -Wno-padded")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-omit-frame-pointer")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -momit-leaf-frame-pointer")

# The yarpl-tests binary constantly fails with an ASAN error in gtest internal
# code on macOS.
if(APPLE AND ${CMAKE_CXX_COMPILER_ID} MATCHES Clang)
  message("== macOS detected, disabling ASAN for yarpl")
  add_compile_options("-fno-sanitize=address,undefined")
endif()

# Using NDEBUG in Release builds.
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -DNDEBUG")

find_path(GLOG_INCLUDE_DIR glog/logging.h)
find_library(GLOG_LIBRARY glog)

message("glog include_dir <${GLOG_INCLUDE_DIR}> lib <${GLOG_LIBRARY}>")

include_directories(SYSTEM ${GLOG_INCLUDE_DIR})
include_directories(${CMAKE_SOURCE_DIR})

# library source
add_library(
        yarpl
        # public API
        include/yarpl/Scheduler.h
        include/yarpl/Disposable.h
        include/yarpl/Refcounted.h
        # Flowable public API
        include/yarpl/Flowable.h
        include/yarpl/flowable/Flowable.h
        include/yarpl/flowable/FlowableOperator.h
        include/yarpl/flowable/Flowable_FromObservable.h
        include/yarpl/flowable/Flowables.h
        include/yarpl/flowable/Subscriber.h
        include/yarpl/flowable/Subscribers.h
        include/yarpl/flowable/Subscription.h
        include/yarpl/flowable/TestSubscriber.h
        src/yarpl/flowable/sources/Subscription.cpp
        # Observable public API
        include/yarpl/Observable.h
        include/yarpl/observable/Observable.h
        include/yarpl/observable/Observables.h
        include/yarpl/observable/ObservableOperator.h
        include/yarpl/observable/Observer.h
        include/yarpl/observable/Observers.h
        include/yarpl/observable/Subscription.h
        include/yarpl/observable/Subscriptions.h
        include/yarpl/observable/TestObserver.h
        src/yarpl/observable/Subscriptions.cpp
        # Single
        include/yarpl/Single.h
        include/yarpl/single/Single.h
        include/yarpl/single/Singles.h
        include/yarpl/single/SingleOperator.h
        include/yarpl/single/SingleObserver.h
        include/yarpl/single/SingleObservers.h
        include/yarpl/single/SingleSubscription.h
        include/yarpl/single/SingleSubscriptions.h
        include/yarpl/single/SingleTestObserver.h
        # utils
        include/yarpl/utils/type_traits.h
        include/yarpl/utils/credits.h
        src/yarpl/utils/credits.cpp
        include/yarpl/utils/ExceptionString.h
        # Scheduler
        include/yarpl/schedulers/ThreadScheduler.h
        src/yarpl/schedulers/ThreadScheduler.cpp)

target_include_directories(
        yarpl
        PUBLIC "${PROJECT_SOURCE_DIR}/include" # allow include paths such as "yarpl/observable.h"
        PUBLIC "${PROJECT_SOURCE_DIR}/src" # allow include paths such as "yarpl/flowable/FlowableRange.h"
        )

target_link_libraries(
  yarpl
  folly
  ${GLOG_LIBRARY})

# Executable for experimenting.
add_executable(
  yarpl-playground
  examples/yarpl-playground.cpp
  examples/FlowableExamples.cpp
  examples/FlowableExamples.h)

target_link_libraries(yarpl-playground yarpl)

# Unit tests.
add_executable(
  yarpl-tests
  test/FlowableTest.cpp
  test/Observable_test.cpp
  test/RefcountedTest.cpp
  test/ReferenceTest.cpp
  test/Scheduler_test.cpp
  test/Single_test.cpp
  test/Tuple.cpp
  test/Tuple.h
  test/credits-test.cpp
  test/yarpl-tests.cpp)

target_link_libraries(
  yarpl-tests
  yarpl
  ${GLOG_LIBRARY}

  # Inherited from rsocket-cpp CMake.
  ${GMOCK_LIBS})

add_dependencies(yarpl-tests gmock)

add_test(NAME yarpl-tests COMMAND yarpl-tests)
