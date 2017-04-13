#
# This config is designed to capture all compiler and linker definitions and search parameters
#

#
# See:
# http://stackoverflow.com/questions/32752446/using-compiler-prefix-commands-with-cmake-distcc-ccache
#
# There are better ways to do this
option(CMAKE_USE_CCACHE "Use 'ccache' as a preprocessor" OFF)
option(CMAKE_USE_DISTCC "Use 'distcc' as a preprocessor" OFF)

find_program(CCACHE_EXECUTABLE ccache
             PATHS /usr/local/ccache/3.2.5/bin/)
find_program(DISTCC_EXECUTABLE distcc)

if (CMAKE_USE_DISTCC AND DISTCC_EXECUTABLE)

    message(STATUS "Enabling distcc: ${DISTCC_EXECUTABLE}")
    set(CMAKE_C_COMPILER_ARG1 ${CMAKE_C_COMPILER})
    set(CMAKE_CXX_COMPILER_ARG1 ${CMAKE_CXX_COMPILER})

    set(CMAKE_C_COMPILER ${DISTCC_EXECUTABLE})
    set(CMAKE_CXX_COMPILER ${DISTCC_EXECUTABLE})

elseif(CMAKE_USE_CCACHE AND CCACHE_EXECUTABLE)

    set(CMAKE_C_COMPILER_ARG1 ${CMAKE_C_COMPILER})
    set(CMAKE_CXX_COMPILER_ARG1 ${CMAKE_CXX_COMPILER})

    set(CMAKE_C_COMPILER ${CCACHE_EXECUTABLE})
    set(CMAKE_CXX_COMPILER ${CCACHE_EXECUTABLE})

    set(CMAKE_C_COMPILE_OBJECT "CCACHE_BASEDIR=${top_src_dir} ${CMAKE_C_COMPILE_OBJECT}")
    set(CMAKE_CXX_COMPILE_OBJECT "CCACHE_BASEDIR=${top_src_dir} ${CMAKE_CXX_COMPILE_OBJECT}")

    # pass these back for ccache to pick up
    set(ENV{CCACHE_BASEDIR} ${top_src_dir})
    message(STATUS "Enabling ccache: ${CCACHE_EXECUTABLE}")
    message(STATUS "  ccache basedir: ${top_src_dir}")

endif()


if ("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
    add_definitions(-D_DEBUG -gdwarf-3)
ELSE()
    add_definitions(-DNDEBUG)
ENDIF()

if (APPLE)
  add_definitions(-D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64)
else (APPLE)
  add_definitions(-D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64)
endif (APPLE)

if (CMAKE_COMPILER_IS_GNUCC)
    add_definitions(-Wall -Wno-format-y2k )
endif()

include(CheckCXXCompilerFlag)
CHECK_CXX_COMPILER_FLAG("-std=c++11" COMPILER_SUPPORTS_CXX11)
CHECK_CXX_COMPILER_FLAG("-std=c++0x" COMPILER_SUPPORTS_CXX0X)
if(COMPILER_SUPPORTS_CXX11)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
elseif(COMPILER_SUPPORTS_CXX0X)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x")
else()
    message(STATUS "The compiler ${CMAKE_CXX_COMPILER} has no C++11 support.")
endif()

#
# NOTE:
# uncomment this for strict mode for library compilation
#
#set(CMAKE_SHARED_LINKER_FLAGS_DYNAMIC "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--export-dynamic")

set(CMAKE_SHARED_LINKER_FLAGS_RDYNAMIC "${CMAKE_SHARED_LINKER_FLAGS}") # for smooth transition, please don't use
set(CMAKE_SHARED_LINKER_FLAGS_ALLOW_UNDEFINED "${CMAKE_SHARED_LINKER_FLAGS}")
if ((NOT DEFINED ${APPLE}) OR (NOT ${APPLE}))
  set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--no-undefined")
endif ()
