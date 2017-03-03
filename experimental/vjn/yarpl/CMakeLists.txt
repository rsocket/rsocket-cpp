project(yarpl)
file(GLOB_RECURSE SOURCES src/*.cc include/*.h )
add_library(${PROJECT_NAME} ${SOURCES})
find_package(Boost REQUIRED COMPONENTS filesystem regex system)
target_link_libraries(${PROJECT_NAME} ${Boost_LIBRARIES} glog gflags)
target_include_directories(
  ${PROJECT_NAME} PUBLIC "${PROJECT_SOURCE_DIR}/include")

project(yarpl-main)
file(GLOB_RECURSE MAIN_SOURCES main/*.cc main/*.h)
add_executable(${PROJECT_NAME} ${MAIN_SOURCES})
target_link_libraries(${PROJECT_NAME} yarpl)

project(yarpl-test)
file(GLOB_RECURSE TEST_SOURCES test/*.cc test/*.h)
add_executable(${PROJECT_NAME} ${TEST_SOURCES})
target_link_libraries(${PROJECT_NAME} yarpl gtest gmock_main)

project(yarpl-benchmark)
file(GLOB_RECURSE BENCHMARK_SOURCES benchmark/*.cc benchmark/*.h)
add_executable(${PROJECT_NAME} ${BENCHMARK_SOURCES})
target_link_libraries(${PROJECT_NAME} yarpl benchmark)
