cmake_minimum_required (VERSION 3.4)
project (yarpl)

# CMake Config

add_definitions(-std=c++14)

# Generate compilation database
set(CMAKE_EXPORT_COMPILE_COMMANDS 1)

# Common configuration for all build modes.
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wno-unused-parameter")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-omit-frame-pointer -momit-leaf-frame-pointer")

# Configuration for Debug build mode.
#set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fsanitize=address")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")

include_directories(${CMAKE_SOURCE_DIR})


# library source
add_library(
        yarpl
        include/yarpl/Observable.h
        include/yarpl/Observable_Observer.h
        include/yarpl/Observable_Subscription.h
        src/Observable_Subscription.cpp
        include/yarpl/flowable.h
        include/yarpl/map.h
        include/yarpl/Flowable_TestSubscriber.h
        include/yarpl/type_traits.h
        # move this out at some point
        include/reactivestreams/ReactiveStreams.h
)

target_include_directories(
        yarpl
        PUBLIC "${PROJECT_SOURCE_DIR}/include" # allow include paths such as "yarpl/observable.h" can be used
)

# Executable for Experimenting
add_executable(
        yarpl-playground
        examples/yarpl-playground.cpp
        examples/ObservableExamples.cpp
        examples/FlowableExamples.cpp
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
        test/Flowable_test.cpp
        test/Subscription_test.cpp
)

target_link_libraries(
        yarpl-tests
        yarpl
        ${GMOCK_LIBS} # inherited from reactivesocket-cpp CMake
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
