
##########################################################
# Google benchmark

ExternalProject_Add(
    google_benchmark
    URL ${CMAKE_SOURCE_DIR}/benchmark-1.1.0.zip
    URL_MD5 c3c5cca410a1959efc93946f1739547f
    CMAKE_ARGS "-DCMAKE_BUILD_TYPE=Release"
    INSTALL_COMMAND ""
)

ExternalProject_Get_Property(google_benchmark source_dir)
set(GOOGLE_BENCHMARK_SOURCE_DIR ${source_dir})
ExternalProject_Get_Property(google_benchmark binary_dir)
set(GOOGLE_BENCHMARK_BINARY_DIR ${binary_dir})

set(
    GOOGLE_BENCHMARK_LIBS
    ${GOOGLE_BENCHMARK_BINARY_DIR}/src/${CMAKE_STATIC_LIBRARY_PREFIX}benchmark${CMAKE_STATIC_LIBRARY_SUFFIX}
)

include_directories(${GOOGLE_BENCHMARK_SOURCE_DIR}/include)
include_directories(${CMAKE_SOURCE_DIR}/experimental)

function(benchmark name file)
    add_executable(${name} ${file})
    target_link_libraries(
        ${name}
        ReactiveSocket
        yarpl
        ${GOOGLE_BENCHMARK_LIBS}
        ${FOLLY_LIBRARIES}
        ${GFLAGS_LIBRARY}
        ${GLOG_LIBRARY}
        ${CMAKE_THREAD_LIBS_INIT})
    add_dependencies(
        ${name}
        ReactiveStreams
        google_benchmark)
endfunction()

benchmark(baselines Baselines.cpp)
benchmark(streamthroughput StreamThroughput.cpp)
benchmark(reqrespthroughput RequestResponseThroughput.cpp)
benchmark(reqresplatency RequestResponseLatency.cpp)
