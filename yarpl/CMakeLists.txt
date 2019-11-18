cmake_minimum_required (VERSION 3.2)
project (yarpl)

# CMake Config
set(CMAKE_MODULE_PATH
  ${CMAKE_CURRENT_SOURCE_DIR}/../cmake/
  # For shipit-transformed builds
  "${CMAKE_CURRENT_SOURCE_DIR}/../build/fbcode_builder/CMake"
  ${CMAKE_MODULE_PATH}
)

add_definitions(-std=c++14)
option(BUILD_TESTS "BUILD_TESTS" ON)

# Generate compilation database
set(CMAKE_EXPORT_COMPILE_COMMANDS 1)

# Common configuration for all build modes.
if (NOT MSVC)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wno-unused-parameter")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-weak-vtables -Wno-padded")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-omit-frame-pointer")
  include(CheckCXXCompilerFlag)
  CHECK_CXX_COMPILER_FLAG("-momit-leaf-frame-pointer" HAVE_OMIT_LEAF_FRAME_POINTER)
  if(HAVE_OMIT_LEAF_FRAME_POINTER)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -momit-leaf-frame-pointer")
  endif()
endif()

if(YARPL_WRAP_SHARED_IN_LOCK)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DYARPL_WRAP_SHARED_IN_LOCK")
    message("Compiler lacks support std::atomic<std::shared_ptr>; wrapping with a mutex")
elseif(YARPL_WRAP_SHARED_IN_ATOMIC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DYARPL_WRAP_SHARED_IN_ATOMIC")
    message("Compiler lacks std::shared_ptr atomic overloads; wrapping in std::atomic")
else()
    message("Compiler has atomic std::shared_ptr support")
endif()


if(${CMAKE_CXX_COMPILER_ID} MATCHES GNU)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -latomic")
endif()

# The yarpl-tests binary constantly fails with an ASAN error in gtest internal
# code on macOS.
if(APPLE AND ${CMAKE_CXX_COMPILER_ID} MATCHES Clang)
  message("== macOS detected, disabling ASAN for yarpl")
  add_compile_options("-fno-sanitize=address,undefined")
endif()

# Using NDEBUG in Release builds.
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -DNDEBUG")

find_package(Gflags REQUIRED)
find_package(Glog REQUIRED)
find_package(fmt CONFIG REQUIRED)

IF(NOT FOLLY_VERSION)
  include(${CMAKE_CURRENT_SOURCE_DIR}/../cmake/InstallFolly.cmake)
ENDIF()

include_directories(SYSTEM ${GFLAGS_INCLUDE_DIR})

# library source
add_library(
        yarpl
        # public API
        Refcounted.h
        Common.h
        # Flowable public API
        Flowable.h
        flowable/DeferFlowable.h
        flowable/EmitterFlowable.h
        flowable/Flowable.h
        flowable/FlowableOperator.h
        flowable/FlowableConcatOperators.h
        flowable/FlowableDoOperator.h
        flowable/FlowableObserveOnOperator.h
        flowable/Flowable_FromObservable.h
        flowable/Flowables.h
        flowable/PublishProcessor.h
        flowable/Subscriber.h
        flowable/Subscription.h
        flowable/TestSubscriber.h
        flowable/Subscription.cpp
        flowable/Flowables.cpp
        # Observable public API
        Observable.h
        observable/DeferObservable.h
        observable/Observable.h
        observable/Observables.h
        observable/ObservableOperator.h
        observable/ObservableConcatOperators.h
        observable/ObservableDoOperator.h
        observable/Observer.h
        observable/Subscription.h
        observable/TestObserver.h
        observable/Subscription.cpp
        observable/Observables.cpp
        # Single
        Single.h
        single/Single.h
        single/Singles.h
        single/SingleOperator.h
        single/SingleObserver.h
        single/SingleObservers.h
        single/SingleSubscription.h
        single/SingleSubscriptions.h
        single/SingleTestObserver.h
        # utils
        utils/credits.h
        utils/credits.cpp)
target_include_directories(
    yarpl
    PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../>
    $<INSTALL_INTERFACE:include>
)

message("yarpl source dir: ${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(
  yarpl
  PUBLIC Folly::folly glog::glog gflags
  INTERFACE ${EXTRA_LINK_FLAGS})

include(CMakePackageConfigHelpers)
configure_package_config_file(
    cmake/yarpl-config.cmake.in
    yarpl-config.cmake
    INSTALL_DESTINATION lib/cmake/yarpl
)
install(TARGETS yarpl EXPORT yarpl-exports DESTINATION lib)
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} DESTINATION include FILES_MATCHING PATTERN "*.h")
install(
    EXPORT yarpl-exports
    NAMESPACE yarpl::
    DESTINATION lib/cmake/yarpl
)
install(
    FILES ${CMAKE_CURRENT_BINARY_DIR}/yarpl-config.cmake
    DESTINATION lib/cmake/yarpl
)

# RSocket's tests also has dependency on this library
add_library(
  yarpl-test-utils
  test_utils/Tuple.cpp
  test_utils/Tuple.h
  test_utils/Mocks.h)

if (BUILD_TESTS)
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
    test/PublishProcessorTest.cpp
    test/SubscribeObserveOnTests.cpp
    test/Single_test.cpp
    test/FlowableSubscriberTest.cpp
    test/credits-test.cpp
    test/yarpl-tests.cpp)

  add_dependencies(yarpl-tests gmock)
  target_link_libraries(
    yarpl-tests
    yarpl
    yarpl-test-utils
    glog::glog
    gflags

    # Inherited from rsocket-cpp CMake.
    ${GMOCK_LIBS}  # This also needs the preceding `add_dependencies`
  )

  add_dependencies(yarpl-tests yarpl-test-utils gmock)

  add_test(NAME yarpl-tests COMMAND yarpl-tests)
endif()
