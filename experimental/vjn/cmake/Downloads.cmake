include (cmake/download/DownloadProject.cmake)

download_project(PROJ   benchmark
    GIT_REPOSITORY      git@github.com:google/benchmark.git
    GIT_TAG             master
    UPDATE_DISCONNECTED 1)
add_subdirectory(${benchmark_SOURCE_DIR} ${benchmark_BINARY_DIR})

download_project(PROJ   double-conversion
    GIT_REPOSITORY      git@github.com:google/double-conversion.git
    GIT_TAG             master
    UPDATE_DISCONNECTED 1)
add_subdirectory(
  ${double-conversion_SOURCE_DIR} ${double-conversion_BINARY_DIR})

set(WITH_GFLAGS OFF CACHE BOOL "Disable in child .cmake.")
download_project(PROJ   gflags
    GIT_REPOSITORY      git@github.com:gflags/gflags.git
    GIT_TAG             master
    UPDATE_DISCONNECTED 1)
add_subdirectory(${gflags_SOURCE_DIR} ${gflags_BINARY_DIR})

download_project(PROJ   glog
    GIT_REPOSITORY      git@github.com:google/glog.git
    GIT_TAG             master
    UPDATE_DISCONNECTED 1)
add_subdirectory(${glog_SOURCE_DIR} ${glog_BINARY_DIR})

download_project(PROJ   googletest
    GIT_REPOSITORY      git@github.com:google/googletest.git
    GIT_TAG             master
    UPDATE_DISCONNECTED 1)
add_subdirectory(${googletest_SOURCE_DIR} ${googletest_BINARY_DIR})

download_project(PROJ   sparsehash
    GIT_REPOSITORY      git@github.com:sparsehash/sparsehash.git
    GIT_TAG             master
    UPDATE_DISCONNECTED 1)
