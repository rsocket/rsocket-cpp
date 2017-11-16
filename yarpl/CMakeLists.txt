cmake_minimum_required (VERSION 3.2)

# To debug the project, set the build type.
set(CMAKE_BUILD_TYPE Debug)

project (yarpl)

# CMake Config
set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../cmake/)

add_definitions(-std=c++14)
option(BUILD_TESTS "BUILD_TESTS" ON)

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

option(YARPL_REFCOUNT_DEBUGGING "Enable refcount debugging/leak checking in Yarpl" OFF)
if(YARPL_REFCOUNT_DEBUGGING)
  add_compile_options(-DYARPL_REFCOUNT_DEBUGGING)
endif()

# Using NDEBUG in Release builds.
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -DNDEBUG")

find_path(GLOG_INCLUDE_DIR glog/logging.h)
find_library(GLOG_LIBRARY glog)

IF(NOT FOLLY_VERSION)
  include(${CMAKE_CURRENT_SOURCE_DIR}/../cmake/InstallFolly.cmake)
ENDIF()

message("glog include_dir <${GLOG_INCLUDE_DIR}> lib <${GLOG_LIBRARY}>")

include_directories(SYSTEM ${GLOG_INCLUDE_DIR})
include_directories(${CMAKE_SOURCE_DIR})

# library source
add_library(
        yarpl
        # public API
        include/yarpl/Refcounted.h
        src/yarpl/Refcounted.cpp
        # Flowable public API
        include/yarpl/Flowable.h
        include/yarpl/flowable/EmitterFlowable.h
        include/yarpl/flowable/Flowable.h
        include/yarpl/flowable/FlowableOperator.h
        include/yarpl/flowable/FlowableObserveOnOperator.h
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
        include/yarpl/observable/ObservableDoOperator.h
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
        src/yarpl/utils/credits.cpp)

target_include_directories(
        yarpl
        PUBLIC "${PROJECT_SOURCE_DIR}/include" # allow include paths such as "yarpl/observable.h"
        PUBLIC "${PROJECT_SOURCE_DIR}/src" # allow include paths such as "yarpl/flowable/FlowableRange.h"
        )

target_link_libraries(
  yarpl
  folly
  ${GLOG_LIBRARY})

install(TARGETS yarpl DESTINATION lib)
install(DIRECTORY include/yarpl DESTINATION include
  FILES_MATCHING PATTERN "*.h")

if (BUILD_TESTS)
  add_library(
    yarpl-test-utils
    test/yarpl/test_utils/Tuple.cpp
    test/yarpl/test_utils/Tuple.h
    test/yarpl/test_utils/utils.h
    test/yarpl/test_utils/Mocks.h)

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
    test/MocksTest.cpp
    test/FlowableTest.cpp
    test/FlowableFlatMapTest.cpp
    test/Observable_test.cpp
    test/RefcountedTest.cpp
    test/ReferenceTest.cpp
    test/SubscribeObserveOnTests.cpp
    test/Single_test.cpp
    test/FlowableSubscriberTest.cpp
    test/credits-test.cpp
    test/yarpl-tests.cpp)

  target_link_libraries(
    yarpl-tests
    yarpl
    yarpl-test-utils
    ${GLOG_LIBRARY}

    # Inherited from rsocket-cpp CMake.
    ${GMOCK_LIBS})

  add_dependencies(yarpl-tests yarpl-test-utils gmock)

  add_test(NAME yarpl-tests COMMAND yarpl-tests)
endif()
