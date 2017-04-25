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
        # Observable public API
        include/yarpl/Observable.h
        include/yarpl/Observable_Observer.h
        include/yarpl/Observable_Subscription.h
        include/yarpl/Observable_TestObserver.h
        src/yarpl/observable/Observable_Subscription.cpp
        # Observable sources
        src/yarpl/observable/sources/Observable_RangeSubscription.h
        src/yarpl/observable/sources/Observable_RangeSubscription.cpp
        # Observable operators
        src/yarpl/observable/operators/Observable_Map.h
        src/yarpl/observable/operators/Observable_Take.h
        # Flowable public API
        include/yarpl/Flowable.h
        include/yarpl/FlowableA.h
        include/yarpl/FlowableB.h
        include/yarpl/FlowableC.h
        include/yarpl/Flowable_Subscriber.h
        include/yarpl/Flowable_SubscriptionSync.h
        include/yarpl/Flowable_TestSubscriber.h
        # hack-y extraction of test code
        include/yarpl/Tuple.h
        # Flowable sources
        src/yarpl/flowable/sources/Flowable_RangeSubscription.h
        src/yarpl/flowable/sources/Flowable_RangeSubscription.cpp
        src/yarpl/flowable/sources/Flowable_FromObservable.h
        # Flowable operators
        src/yarpl/flowable/operators/Flowable_Map.h
        src/yarpl/flowable/operators/Flowable_Take.h
        src/yarpl/flowable/operators/Flowable_SubscribeOn.h
        # Flowable utils
        src/yarpl/flowable/utils/SubscriptionHelper.h
        src/yarpl/flowable/utils/SubscriptionHelper.cpp
        # utils
        src/yarpl/utils/type_traits.h
        # Scheduler
        src/yarpl/ThreadScheduler.cpp
        src/yarpl/ThreadScheduler.h
        # move this out at some point
        include/reactivestreams/ReactiveStreams.h
        # hack-y extraction of test code
        src/yarpl/Tuple.cpp
        # The "v" version of Flowable.
        include/yarpl/v/Flowable.h
        include/yarpl/v/Flowables.h
        include/yarpl/v/FlowableV.h
        include/yarpl/v/Operator.h
        include/yarpl/v/Refcounted.h
        include/yarpl/v/Subscriber.h
        include/yarpl/v/Subscribers.h
        include/yarpl/v/Subscription.h
        src/yarpl/v/Refcounted.cpp
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
        examples/ObservableExamples.cpp
        examples/FlowableExamples.cpp
        examples/FlowableExamples.h
        examples/FlowableAExamples.cpp
        examples/FlowableAExamples.h
        examples/FlowableBExamples.cpp
        examples/FlowableBExamples.h
        examples/FlowableCExamples.cpp
        examples/FlowableCExamples.h
        examples/FlowableVExamples.cpp
        examples/FlowableVExamples.h
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
        test/Observable_test.cpp
        test/FlowableCreateSubscribe_test.cpp
        test/Observable_Subscription_test.cpp
        test/Flowable_range_test.cpp
        test/FlowableSubscriptionHelper_test.cpp
        test/Flowable_lifecycle.cpp
        test/FlowableA_lifecycle.cpp
        test/FlowableB_lifecycle.cpp
        test/FlowableChaining_test.cpp
        test/flowable_operators/Flowable_Map_test.cpp
        test/flowable_operators/Flowable_Take_test.cpp
        test/flowable_operators/Flowable_SubscribeOn_test.cpp
        test/Scheduler_test.cpp
        test/v/RefcountedTest.cpp
        test/v/FlowableTest.cpp
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
