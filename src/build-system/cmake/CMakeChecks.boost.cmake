# Boost: headers and libs [use as ${BOOST_LIBPATH} ${BOOST_*_LIBS} ${RT_LIBS}]

if (BUILD_SHARED_LIBS)
    set(Boost_USE_STATIC_LIBS       OFF)
    set(Boost_USE_STATIC_RUNTIME    OFF)
    add_definitions(-DBOOST_LOG_DYN_LINK)
else()
    set(Boost_USE_STATIC_LIBS       ON)
    set(Boost_USE_STATIC_RUNTIME    ON)
endif()
set(Boost_USE_MULTITHREADED     ON)

#Hints for FindBoost
if (EXISTS "${NCBI_TOOLS_ROOT}/boost-1.57.0-ncbi1")
    set(BOOST_ROOT ${NCBI_TOOLS_ROOT}/boost-1.57.0-ncbi1 )
endif()

find_package(Boost
             COMPONENTS filesystem regex system)
if (NOT Boost_FOUND)
    message(FATAL "BOOST not found...") 
  #XXX WARNING
endif (NOT Boost_FOUND)

set(BOOST_INCLUDE ${Boost_INCLUDE_DIRS})
set(BOOST_LIBPATH -L${Boost_LIBRARY_DIRS} -Wl,-rpath,${Boost_LIBRARY_DIRS})
