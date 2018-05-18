if (NOT FOLLY_INSTALL_DIR)
  set(FOLLY_INSTALL_DIR $ENV{HOME}/folly)
endif ()

# Check if the correct version of folly is already installed.
set(FOLLY_VERSION v2018.05.14.00)
set(FOLLY_VERSION_FILE ${FOLLY_INSTALL_DIR}/${FOLLY_VERSION})
if (RSOCKET_INSTALL_DEPS)
  if (NOT EXISTS ${FOLLY_VERSION_FILE})
    # Remove the old version of folly.
    file(REMOVE_RECURSE ${FOLLY_INSTALL_DIR})
    set(INSTALL_FOLLY True)
  endif ()
endif ()

if (INSTALL_FOLLY)
  # Build and install folly.
  ExternalProject_Add(
    folly-ext
    GIT_REPOSITORY https://github.com/facebook/folly
    GIT_TAG ${FOLLY_VERSION}
    BINARY_DIR folly-ext-prefix/src/folly-ext/folly
    CONFIGURE_COMMAND autoreconf -ivf
      COMMAND ./configure CXX=${CMAKE_CXX_COMPILER}
                          --prefix=${FOLLY_INSTALL_DIR}
    BUILD_COMMAND make -j4
    INSTALL_COMMAND make install
      COMMAND cmake -E touch ${FOLLY_VERSION_FILE})

  set(FOLLY_INCLUDE_DIR ${FOLLY_INSTALL_DIR}/include)
  set(lib ${CMAKE_SHARED_LIBRARY_PREFIX}folly${CMAKE_SHARED_LIBRARY_SUFFIX})
  set(benchlib ${CMAKE_SHARED_LIBRARY_PREFIX}follybenchmark${CMAKE_SHARED_LIBRARY_SUFFIX})
  set(FOLLY_LIBRARY ${FOLLY_INSTALL_DIR}/lib/${lib})
  set(FOLLY_BENCHMARK_LIBRARY ${FOLLY_INSTALL_DIR}/lib/${benchlib})

  # CMake requires directories listed in INTERFACE_INCLUDE_DIRECTORIES to exist.
  file(MAKE_DIRECTORY ${FOLLY_INCLUDE_DIR})
else ()
  # Use installed folly.
  find_package(Folly REQUIRED)
endif ()

find_package(Threads)
find_library(EVENT_LIBRARY event)

add_library(folly SHARED IMPORTED)
set_property(TARGET folly PROPERTY IMPORTED_LOCATION ${FOLLY_LIBRARY})
set_property(TARGET folly
  APPEND PROPERTY INTERFACE_LINK_LIBRARIES
  ${EXTRA_LINK_FLAGS} ${EVENT_LIBRARY} ${CMAKE_THREAD_LIBS_INIT})
if (TARGET folly-ext)
  add_dependencies(folly folly-ext)
endif ()

add_library(folly-benchmark SHARED IMPORTED)
set_property(TARGET folly-benchmark PROPERTY IMPORTED_LOCATION ${FOLLY_BENCHMARK_LIBRARY})
set_property(TARGET folly-benchmark
  APPEND PROPERTY INTERFACE_LINK_LIBRARIES
  ${EXTRA_LINK_FLAGS} ${EVENT_LIBRARY} ${CMAKE_THREAD_LIBS_INIT})
if (TARGET folly-ext)
  add_dependencies(folly-benchmark folly-ext)
endif ()

# Folly includes are marked as system to prevent errors on non-standard
# extensions when compiling with -pedantic and -Werror.
set_property(TARGET folly
  APPEND PROPERTY INTERFACE_SYSTEM_INCLUDE_DIRECTORIES ${FOLLY_INCLUDE_DIR})
set_property(TARGET folly
  APPEND PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${FOLLY_INCLUDE_DIR})
set_property(TARGET folly-benchmark
  APPEND PROPERTY INTERFACE_SYSTEM_INCLUDE_DIRECTORIES ${FOLLY_INCLUDE_DIR})
set_property(TARGET folly-benchmark
  APPEND PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${FOLLY_INCLUDE_DIR})
