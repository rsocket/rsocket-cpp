cmake_minimum_required(VERSION 3.2)

include(FindPackageHandleStandardArgs)

if (FOLLY_INSTALL_DIR)
  set(lib_paths ${FOLLY_INSTALL_DIR}/lib)
  set(include_paths ${FOLLY_INSTALL_DIR}/include)
endif ()

find_library(FOLLY_LIBRARY folly PATHS ${lib_paths})
find_path(FOLLY_INCLUDE_DIR "folly/String.h" PATHS ${include_paths})

find_package_handle_standard_args(Folly
  DEFAULT_MSG FOLLY_LIBRARY FOLLY_INCLUDE_DIR)
