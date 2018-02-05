cmake_minimum_required(VERSION 3.2.1)
project(rs)

shk_library(
  NAME rs
  SOURCES
    include/rs/all.h
    include/rs/append.h
    include/rs/average.h
    include/rs/catch.h
    include/rs/concat.h
    include/rs/concat_map.h
    include/rs/contains.h
    include/rs/count.h
    include/rs/default_if_empty.h
    include/rs/detail/optional.h
    include/rs/element_at.h
    include/rs/element_count.h
    include/rs/empty.h
    include/rs/end_with.h
    include/rs/filter.h
    include/rs/first.h
    include/rs/from.h
    include/rs/if_empty.h
    include/rs/is_empty.h
    include/rs/just.h
    include/rs/last.h
    include/rs/map.h
    include/rs/max.h
    include/rs/merge.h
    include/rs/min.h
    include/rs/never.h
    include/rs/pipe.h
    include/rs/prepend.h
    include/rs/publisher.h
    include/rs/range.h
    include/rs/reduce.h
    include/rs/repeat.h
    include/rs/scan.h
    include/rs/skip.h
    include/rs/skip_while.h
    include/rs/some.h
    include/rs/splat.h
    include/rs/start.h
    include/rs/start_with.h
    include/rs/subscriber.h
    include/rs/subscription.h
    include/rs/sum.h
    include/rs/take.h
    include/rs/take_while.h
    include/rs/throw.h
    include/rs/weak_reference.h
    include/rs/zip.h
    src/subscriber.cpp
    src/subscription.cpp
  PUBLIC_INCLUDES
    include)

shk_test(
  NAME rs-test
  SOURCES
    test/all_test.cpp
    test/append_test.cpp
    test/average_test.cpp
    test/backpressure_violator.h
    test/catch_test.cpp
    test/concat_map_test.cpp
    test/concat_test.cpp
    test/contains_test.cpp
    test/count_test.cpp
    test/default_if_empty_test.cpp
    test/detail/optional_test.cpp
    test/element_at_test.cpp
    test/element_count_test.cpp
    test/empty_test.cpp
    test/end_with_test.cpp
    test/filter_test.cpp
    test/first_test.cpp
    test/from_test.cpp
    test/if_empty_test.cpp
    test/is_empty_test.cpp
    test/just_test.cpp
    test/last_test.cpp
    test/main.cpp
    test/map_test.cpp
    test/max_test.cpp
    test/merge_test.cpp
    test/min_test.cpp
    test/never_test.cpp
    test/pipe_test.cpp
    test/prepend_test.cpp
    test/publisher_test.cpp
    test/range_test.cpp
    test/reduce_test.cpp
    test/repeat_test.cpp
    test/scan_test.cpp
    test/skip_test.cpp
    test/skip_while_test.cpp
    test/some_test.cpp
    test/splat_test.cpp
    test/start_test.cpp
    test/start_with_test.cpp
    test/subscriber_test.cpp
    test/subscription_test.cpp
    test/take_test.cpp
    test/take_while_test.cpp
    test/sum_test.cpp
    test/test_util.h
    test/throw_test.cpp
    test/weak_reference_test.cpp
    test/zip_test.cpp
  DEPENDENCIES
    rs folly)

target_compile_options(rs PUBLIC "-Wno-unused-parameter")

message("Catch at: ${CATCH_INCLUDE_DIR}")
add_dependencies(rs-test catch)
target_include_directories(rs-test PUBLIC "${CATCH_INCLUDE_DIR}")