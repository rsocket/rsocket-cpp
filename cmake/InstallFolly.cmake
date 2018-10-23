if (NOT FOLLY_INSTALL_DIR)
  set(FOLLY_INSTALL_DIR ${CMAKE_BINARY_DIR}/folly-install)
endif ()

if (RSOCKET_INSTALL_DEPS)
  execute_process(
    COMMAND
      ${CMAKE_SOURCE_DIR}/scripts/build_folly.sh
      ${CMAKE_BINARY_DIR}/folly-src
      ${FOLLY_INSTALL_DIR}
    RESULT_VARIABLE folly_result
  )
  if (NOT "${folly_result}" STREQUAL "0")
    message(FATAL_ERROR "failed to build folly")
  endif()
endif ()

find_package(Threads)
find_package(folly CONFIG REQUIRED PATHS ${FOLLY_INSTALL_DIR})
