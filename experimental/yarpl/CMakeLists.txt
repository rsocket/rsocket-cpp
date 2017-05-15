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
        include/yarpl/Refcounted.h
        # Flowable public API
        include/yarpl/Flowable.h
        include/yarpl/Flowables.h
        include/yarpl/flowable/FlowableOperator.h
        include/yarpl/flowable/Subscriber.h
        include/yarpl/flowable/Subscribers.h
        include/yarpl/flowable/Subscription.h
        # Observable public API
        include/yarpl/Observable.h
        include/yarpl/Observables.h
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
        src/yarpl/single/SingleSubscriptions.cpp
        # Flowable private
        src/yarpl/Refcounted.cpp
        src/yarpl/flowable/sources/Flowable_FromObservable.h
        # utils
        include/yarpl/utils/type_traits.h
        src/yarpl/flowable/utils/SubscriptionHelper.h
        src/yarpl/flowable/utils/SubscriptionHelper.cpp
        # Scheduler
        include/yarpl/ThreadScheduler.h
        src/yarpl/ThreadScheduler.cpp
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
        test/Observable_test.cpp
        test/Single_test.cpp
#        test/FlowableCreateSubscribe_test.cpp
#        test/Observable_Subscription_test.cpp
#        test/Flowable_range_test.cpp
#        test/Flowable_lifecycle.cpp
#        test/FlowableChaining_test.cpp
        test/FlowableTest.cpp
        test/RefcountedTest.cpp
        test/ReferenceTest.cpp
        test/Scheduler_test.cpp
        test/SubscriptionHelper_test.cpp
        test/Tuple.cpp
        test/Tuple.h
)

target_link_libraries(
        yarpl-tests
        yarpl
        ${FOLLY_LIBRARIES} # inherited from rsocket-cpp CMake
        ${GMOCK_LIBS} # inherited from rsocket-cpp CMake
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
