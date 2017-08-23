##########################################################
# Google benchmark

benchmark(reqresplatency RequestResponseLatency.cpp)

function(make_folly_benchmark NAME FILE)
  add_executable(${NAME} ${FILE} Benchmarks.cpp)
  target_link_libraries(
    ${NAME}
    ReactiveSocket
    folly-benchmark
    ${GFLAGS_LIBRARY}
    ${GLOG_LIBRARY})
endfunction()

make_folly_benchmark(baselines_tcp BaselinesTcp.cpp)
make_folly_benchmark(baselines_async_socket BaselinesAsyncSocket.cpp)

make_folly_benchmark(req-response-throughput-tcp RequestResponseThroughputTcp.cpp)
make_folly_benchmark(stream-throughput-tcp StreamThroughputTcp.cpp)
make_folly_benchmark(stream-throughput-mem StreamThroughputMemory.cpp)

add_test(NAME RequestResponseThroughputTcpTest COMMAND req-response-throughput-tcp --items 100000)
add_test(NAME StreamThroughputTcpTest COMMAND stream-throughput-tcp --items 100000)
add_test(NAME StreamThroughputMemoryTest COMMAND stream-throughput-mem --items 100000)
