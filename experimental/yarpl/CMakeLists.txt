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

# Configuration for Debug build mode.
#set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fsanitize=address")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")

set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -DNDEBUG")

include_directories(${CMAKE_SOURCE_DIR})


# library source
add_library(
        yarpl
        # public API
        include/yarpl/Scheduler.h
        include/yarpl/Disposable.h
        # Flowable public API
        include/yarpl/Flowable.h
        include/yarpl/Flowables.h
        include/yarpl/Operator.h
        include/yarpl/Refcounted.h
        include/yarpl/Subscriber.h
        include/yarpl/Subscribers.h
        include/yarpl/Subscription.h
        # Observable public API
        include/yarpl/Observable.h
        include/yarpl/Observable_Observer.h
        include/yarpl/Observable_Subscription.h
        include/yarpl/Observable_TestObserver.h
        src/yarpl/observable/Observable_Subscription.cpp
        # Flowable sources
        src/yarpl/Refcounted.cpp
        # Observable sources
        src/yarpl/observable/sources/Observable_RangeSubscription.h
        src/yarpl/observable/sources/Observable_RangeSubscription.cpp
        # Observable operators
        src/yarpl/observable/operators/Observable_Map.h
        src/yarpl/observable/operators/Observable_Take.h
        # utils
        src/yarpl/utils/type_traits.h
        # Scheduler
        src/yarpl/ThreadScheduler.cpp
        src/yarpl/ThreadScheduler.h
)

target_include_directories(
        yarpl
        PUBLIC "${PROJECT_SOURCE_DIR}/include" # allow include paths such as "yarpl/observable.h"
        PUBLIC "${PROJECT_SOURCE_DIR}/src" # allow include paths such as "yarpl/flowable/FlowableRange.h"
)

# Executable for Experimenting
add_executable(
        yarpl-playground
        examples/yarpl-playground.cpp
#        examples/ObservableExamples.cpp
        examples/FlowableExamples.cpp
        examples/FlowableExamples.h
)

target_link_libraries(
        yarpl-playground
        yarpl)

target_include_directories(
        yarpl-playground
        PUBLIC "${PROJECT_SOURCE_DIR}/include" # allow include paths such as "yarpl/observable.h" can be used
)

# unit tests
add_executable(
        yarpl-tests
        test/yarpl-tests.cpp
#        test/Observable_test.cpp
#        test/FlowableCreateSubscribe_test.cpp
#        test/Observable_Subscription_test.cpp
#        test/Flowable_range_test.cpp
#        test/Flowable_lifecycle.cpp
#        test/FlowableA_lifecycle.cpp
#        test/FlowableB_lifecycle.cpp
#        test/FlowableChaining_test.cpp
        test/Scheduler_test.cpp
        test/RefcountedTest.cpp
        test/FlowableTest.cpp
)

target_link_libraries(
        yarpl-tests
        yarpl
        ${GMOCK_LIBS} # inherited from reactivesocket-cpp CMake
        ${CMAKE_THREAD_LIBS_INIT}
)

target_include_directories(
        yarpl-tests
        PUBLIC "${PROJECT_SOURCE_DIR}/include" # allow include paths such as "yarpl/observable.h" can be used
)

add_dependencies(yarpl-tests gmock)

## perf tests
#add_executable(
#        yarpl-perf
#        perf/yarpl-perf.cpp
#        perf/Observable_perf.cpp
#        perf/Function_perf.cpp
#)
#
#target_link_libraries(
#        yarpl-perf
#        yarpl
#        benchmark
#)

#target_include_directories(
#        yarpl-perf
#        PUBLIC "${PROJECT_SOURCE_DIR}/include" # allow include paths such as "yarpl/observable.h" can be used
#)
