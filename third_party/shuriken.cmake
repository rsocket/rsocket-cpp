project(shk)

enable_testing()

set(SHK_COMPILER_FLAGS "-Wall -Wimplicit-fallthrough -Werror")

function(shk_cpp_target NAME)
  set_property(TARGET ${NAME} APPEND_STRING PROPERTY COMPILE_FLAGS ${SHK_COMPILER_FLAGS})
  set_property(TARGET ${NAME} PROPERTY CXX_STANDARD 14)
  add_sanitizers(${NAME})
endfunction()

function(shk_library)
  set(OPTIONS)
  set(ONE_VALUE_ARGS NAME)
  set(MULTI_VALUE_ARGS SOURCES DEPENDENCIES PUBLIC_INCLUDES)
  cmake_parse_arguments(shk_library "${OPTIONS}" "${ONE_VALUE_ARGS}" "${MULTI_VALUE_ARGS}" ${ARGN})

  add_library(${shk_library_NAME}
    ${shk_library_SOURCES})
  shk_cpp_target(${shk_library_NAME})
  target_link_libraries(${shk_library_NAME} ${shk_library_DEPENDENCIES})
  target_include_directories(${shk_library_NAME} PUBLIC ${shk_library_PUBLIC_INCLUDES})
endfunction()

function(shk_executable)
  set(OPTIONS USER_FACING)
  set(ONE_VALUE_ARGS NAME)
  set(MULTI_VALUE_ARGS SOURCES DEPENDENCIES PRIVATE_INCLUDES)
  cmake_parse_arguments(shk_executable "${OPTIONS}" "${ONE_VALUE_ARGS}" "${MULTI_VALUE_ARGS}" ${ARGN})

  add_executable(${shk_executable_NAME}
    ${shk_executable_SOURCES})
  shk_cpp_target(${shk_executable_NAME})
  target_link_libraries(${shk_executable_NAME} ${shk_executable_DEPENDENCIES})
  target_include_directories(${shk_executable_NAME} PRIVATE ${shk_executable_PRIVATE_INCLUDES})

  if (${shk_executable_USER_FACING})
    set_target_properties(${shk_executable_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin")
  endif()
endfunction()

function(shk_test)
  set(OPTIONS)
  set(ONE_VALUE_ARGS NAME)
  set(MULTI_VALUE_ARGS)
  cmake_parse_arguments(shk_test "${OPTIONS}" "${ONE_VALUE_ARGS}" "${MULTI_VALUE_ARGS}" ${ARGN})
  shk_executable(${ARGV})
  add_test(NAME ${shk_test_NAME} COMMAND ${shk_test_NAME})
endfunction()