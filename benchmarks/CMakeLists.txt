
##########################################################
# Google benchmark


benchmark(baselines Baselines.cpp)
benchmark(reqrespthroughput RequestResponseThroughput.cpp)
benchmark(reqresplatency RequestResponseLatency.cpp)

function(make_folly_benchmark NAME FILE)
  add_executable(${NAME} ${FILE})
  target_link_libraries(
    ${NAME}
    ReactiveSocket
    folly-benchmark
    ${GFLAGS_LIBRARY}
    ${GLOG_LIBRARY})
endfunction()

make_folly_benchmark(streamthroughput StreamThroughput.cpp)
