include(CMakeParseArguments)

function(update_final_message)
    list(GET ARGN 0 ARG_LIBNAME_ORIG)
    if (${ARGC} GREATER 1)
        list(GET ARGN 1 ARG_PRINT_NAME)
    else()
        set(ARG_PRINT_NAME ${ARG_LIBNAME_ORIG})
    endif()
    string(TOUPPER ${ARG_LIBNAME_ORIG} ARG_LIBNAME)
    if ("${ARG_LIBNAME}_DISABLED" STREQUAL "yes")
        set(EXTERNAL_LIBRARIES_COMMENT "${EXTERNAL_LIBRARIES_COMMENT}\n${ARG_PRINT_NAME}: disabled" PARENT_SCOPE)
    elseif(${${ARG_LIBNAME}_FOUND})
        set(EXTERNAL_LIBRARIES_COMMENT "${EXTERNAL_LIBRARIES_COMMENT}\n${ARG_PRINT_NAME}: ${${ARG_LIBNAME}_LIBS}\n${ARG_PRINT_NAME} include: ${${ARG_LIBNAME}_INCLUDE}" PARENT_SCOPE)
    else()
        set(EXTERNAL_LIBRARIES_COMMENT "${EXTERNAL_LIBRARIES_COMMENT}\n${ARG_PRINT_NAME}: not found" PARENT_SCOPE)
    endif()
endfunction()

function(find_external_library)
    cmake_parse_arguments(ARG "DYNAMIC_ONLY" "PACKAGE_NAME" "EXTRALIBS" ${ARGN})
    #list(LENGTH "${ARG_EXTRALIBS}" COUNT)
    #message("EXTRALIBS: ${ARG_EXTRALIBS} COUNT: ${COUNT}")
    list(GET ARGN 0 ARG_LIBNAME_ORIG)
    string(TOUPPER ${ARG_LIBNAME_ORIG} ARG_LIBNAME)
    if ("${BUILD_SHARED_LIBS}" STREQUAL "OFF" AND NOT ${ARG_DYNAMIC_ONLY})
        set(CMAKE_FIND_LIBRARY_SUFFIXES_OLD "${CMAKE_FIND_LIBRARY_SUFFIXES}")
        set(CMAKE_FIND_LIBRARY_SUFFIXES "${CMAKE_STATIC_LIBRARY_SUFFIX}")
        set("LINKER_FLAG_PREFIX" "-Wl,-Bstatic")
        set("LINKER_FLAG_SUFFIX" "-Wl,-Bdynamic")
        find_external_library_impl("${ARGN}")
        unset(LINKER_FLAG_PREFIX)
        unset(LINKER_FLAG_SUFFIX)
        set(CMAKE_FIND_LIBRARY_SUFFIXES "${CMAKE_FIND_LIBRARY_SUFFIXES_OLD}")
        if (${${ARG_LIBNAME}_FOUND})
        else ()
            #message("STATIC NOT FOUND, LOOKING FOR DYNAMIC")
            find_external_library_impl("${ARGN}")
        endif ()
    else ()
        find_external_library_impl("${ARGN}")
    endif ()
    if (${${ARG_LIBNAME}_FOUND})
        set("${ARG_LIBNAME}_FOUND" ${${ARG_LIBNAME}_FOUND} PARENT_SCOPE)
        set("${ARG_LIBNAME}_INCLUDE" ${${ARG_LIBNAME}_INCLUDE} PARENT_SCOPE)
        #set(LIBS "")
        #list(APPEND LIBS ${${ARG_LIBNAME}_LIBS})
        #list(APPEND LIBS ${ARG_EXTRALIBS})
        #set("${ARG_LIBNAME}_LIBS" ${LIBS} PARENT_SCOPE)

        list(APPEND ${ARG_LIBNAME}_LIBS ${ARG_EXTRALIBS})
        set("${ARG_LIBNAME}_LIBS" "${${ARG_LIBNAME}_LIBS}" PARENT_SCOPE)

        #list(APPEND ARG_EXTRALIBS ${${ARG_LIBNAME}_LIBS})
        #set("${ARG_LIBNAME}_LIBS" "${ARG_EXTRALIBS}" PARENT_SCOPE)

        #set(LIBS "${${ARG_LIBNAME}_LIBS};${ARG_EXTRALIBS}")
        #string (REPLACE ";" " " LIBS_STR "${LIBS}")
        #set("${ARG_LIBNAME}_LIBS" "${LIBS}" PARENT_SCOPE)
        set(TOOLKIT_MODULES_FOUND "${TOOLKIT_MODULES_FOUND} ${ARG_LIBNAME_ORIG}" PARENT_SCOPE)
    endif ()
endfunction()

function(convert_link_string FILES)
    foreach(PATH ${FILES})
        get_filename_component(D "${PATH}" DIRECTORY)
        get_filename_component(FFULL "${PATH}" NAME)
        #set(SUBSTRING "${FFULL}" 3 -1 F)
        string(REGEX MATCH ".a$" ISSTATIC ${FFULL})
        string(REGEX REPLACE "^lib" "" F0 ${FFULL})
        string(REGEX REPLACE ".(so|a)$" "" F ${F0})
        if (NOT ("${D}" STREQUAL "/usr/lib" OR "${D}" STREQUAL "/usr/lib64"))
          if ("${RES}" STREQUAL "")
            set(RES "-L${D}" "-l${F}")
          else ()
            set(RES ${RES} "-L${D}" "-l${F}")
          endif()
        else ()
          if ("${RES}" STREQUAL "")
            set(RES "-l${F}")
          else ()
            set(RES ${RES} "-l${F}")
          endif()
        endif()
    endforeach()
    if (NOT "${ISSTATIC}" STREQUAL "")
        set(CONVERTED_LIB_STRING ${LINKER_FLAG_PREFIX} ${RES} ${LINKER_FLAG_SUFFIX} PARENT_SCOPE)
    else ()
        set(CONVERTED_LIB_STRING ${RES} PARENT_SCOPE)
    endif()
endfunction()

function(find_libraries)
    cmake_parse_arguments(ARG "NO_DEFAULT_PATH;NO_CMAKE_ENVIRONMENT_PATH;NO_CMAKE_PATH;NO_SYSTEM_ENVIRONMENT_PATH;NO_CMAKE_SYSTEM_PATH" "PATH" "NAMES;HINTS" ${ARGN})
    list(GET ARGN 0 OUT_VARIABLE_NAME)
    foreach (LIBNAME ${ARG_NAMES})
        unset(LIB_OUT CACHE)
        find_library(LIB_OUT NAMES ${LIBNAME}
            HINTS ${ARG_HINTS}
            ${ARG_NO_DEFAULT_PATH}
            ${ARG_NO_CMAKE_ENVIRONMENT_PATH}
            ${ARG_NO_CMAKE_PATH}
            ${ARG_NO_SYSTEM_ENVIRONMENT_PATH}
            ${ARG_NO_CMAKE_SYSTEM_PATH})
        if ("${LIB_OUT}" STREQUAL "LIB_OUT-NOTFOUND")
            set(${${OUT_VARIABLE_NAME}} "${OUT_VARIABLE_NAME}-NOTFOUND" PARENT_SCOPE)
            return ()
        endif()
        set(RES ${RES} ${LIB_OUT})
    endforeach()
    #message("OUT: " ${RES})
    set("${OUT_VARIABLE_NAME}" ${RES} PARENT_SCOPE)
endfunction()

function(find_external_library_impl)
    list(GET ARGN 0 ARG_LIBNAME_ORIG)
    string(TOUPPER ${ARG_LIBNAME_ORIG} ARG_LIBNAME)
    cmake_parse_arguments(ARG "DO_NOT_UPDATE_MESSAGE" "INCLUDES;HINTS;INCLUDE_HINTS;LIBS_HINTS;EXTRAFLAGS;PRINT_NAME;PACKAGE_NAME" "LIBS" ${ARGN})
    #message("LIBNAME: ${ARG_LIBNAME}")

    if (NOT "${ARG_HINTS}" STREQUAL "")
        set(ARG_INCLUDE_HINTS "${ARG_HINTS}/include")
        set(ARG_LIBS_HINTS "${ARG_HINTS}/lib")
    endif()

    if ("${ARG_PACKAGE_NAME}" STREQUAL "")
      set(ARG_PACKAGE_NAME "${ARG_LIBNAME_ORIG}")
    endif ()
    string(TOUPPER ${ARG_PACKAGE_NAME} ARG_PACKAGE_NAME_UPPER)

    #debug
    #message("ARG_LIBNAME_ORIG ${ARG_LIBNAME_ORIG} LIBNAME ${ARG_LIBNAME} VALUE ${${ARG_LIBNAME}}")
    #message("HINTS ${ARG_HINTS} INCLUDE_HINTS ${ARG_INCLUDE_HINTS} LIBS_HINTS ${ARG_LIBS_HINTS}")
    #message("INCLUDES ${ARG_INCLUDES} LIBS ${ARG_LIBS} EXTRAFLAGS ${ARG_EXTRAFLAGS}")

    if (NOT ARG_LIBNAME)
        message(FATAL_ERROR "find_external_library: library name not provided")
    endif()

    if ("${ARG_LIBNAME}_DISABLED" STREQUAL "yes")
        set("${ARG_LIBNAME}_FOUND" false)
        return()
    endif()

    #find_library and find_path declare their output variables as  cached (see cmake type system)
    #so, we need to reset them
    unset(EXT_LIBRARY CACHE)
    unset(EXT_INCLUDE_DIR CACHE)

    if (NOT "${${ARG_LIBNAME}}" STREQUAL "")
        find_libraries(EXT_LIBRARY NAMES ${ARG_LIBS}
            PATH "${${ARG_LIBNAME}}/lib"
            NO_DEFAULT_PATH
            NO_CMAKE_ENVIRONMENT_PATH
            NO_CMAKE_PATH
            NO_SYSTEM_ENVIRONMENT_PATH
            NO_CMAKE_SYSTEM_PATH)
        #if (NOT "${EXT_INCLUDE_DIR}" STREQUAL "")
            find_path(EXT_INCLUDE_DIR NAMES ${ARG_INCLUDES}
                PATH "${${ARG_LIBNAME}}/include"
                NO_DEFAULT_PATH
                NO_CMAKE_ENVIRONMENT_PATH
                NO_CMAKE_PATH
                NO_SYSTEM_ENVIRONMENT_PATH
                NO_CMAKE_SYSTEM_PATH)
        #endif()

    elseif (NOT "${ARG_LIBS_HINTS}" STREQUAL "")
        set(CMAKE_PREFIX_PATH_OLD "${CMAKE_PREFIX_PATH}")
        set(CMAKE_PREFIX_PATH "${ARG_HINTS}")
        find_package(${ARG_PACKAGE_NAME} QUIET)
        set(CMAKE_PREFIX_PATH "${CMAKE_PREFIX_PATH_OLD}")

        if (${${ARG_PACKAGE_NAME_UPPER}_FOUND})
            if (NOT "${${ARG_PACKAGE_NAME_UPPER}_INCLUDE_DIRS}" STREQUAL "")
                set(EXT_INCLUDE_DIR "${${ARG_PACKAGE_NAME_UPPER}_INCLUDE_DIRS}")
            endif ()
            if (NOT "${${ARG_PACKAGE_NAME_UPPER}_INCLUDE_DIR}" STREQUAL "")
                set(EXT_INCLUDE_DIR "${${ARG_PACKAGE_NAME_UPPER}_INCLUDE_DIR}")
            endif ()
            set("${ARG_LIBNAME}_FOUND" true PARENT_SCOPE)
            set("${ARG_LIBNAME}_INCLUDE" "${EXT_INCLUDE_DIR}" PARENT_SCOPE)
            if ("${ARG_EXTRAFLAGS}" STREQUAL "")
                set("${ARG_LIBNAME}_LIBS" ${${ARG_PACKAGE_NAME_UPPER}_LIBRARIES} PARENT_SCOPE)
            else ()
                convert_link_string("${${ARG_PACKAGE_NAME_UPPER}_LIBRARIES}")
                set("${ARG_LIBNAME}_LIBS" ${CONVERTED_LIB_STRING} ${ARG_EXTRAFLAGS} PARENT_SCOPE)
            endif ()
            message(STATUS "Found ${ARG_PACKAGE_NAME}: ${${ARG_PACKAGE_NAME_UPPER}_LIBRARIES}")
            return()
        else ()
          #message("${ARG_LIBNAME_ORIG} NOT FOUND AS PACKAGE")
          #message("ARG_LIBS: ${ARG_LIBS} LIB_HINTS: ${ARG_LIBS_HINTS}")
          find_libraries(EXT_LIBRARY NAMES ${ARG_LIBS}
              HINTS
              ${ARG_LIBS_HINTS})
          find_path(EXT_INCLUDE_DIR NAMES ${ARG_INCLUDES}
              HINTS
              ${ARG_INCLUDE_HINTS})
        endif()

    else()
        find_package(${ARG_PACKAGE_NAME} QUIET)
        if (${${ARG_PACKAGE_NAME_UPPER}_FOUND})
            if (NOT "${${ARG_PACKAGE_NAME_UPPER}_INCLUDE_DIRS}" STREQUAL "")
                set(EXT_INCLUDE_DIR "${${ARG_PACKAGE_NAME_UPPER}_INCLUDE_DIRS}")
            endif ()
            if (NOT "${${ARG_PACKAGE_NAME_UPPER}_INCLUDE_DIR}" STREQUAL "")
                set(EXT_INCLUDE_DIR "${${ARG_PACKAGE_NAME_UPPER}_INCLUDE_DIR}")
            endif ()
            #convert_link_string("${${ARG_PACKAGE_NAME_UPPER}_LIBRARIES}")
            set("${ARG_LIBNAME}_FOUND" true PARENT_SCOPE)
            set("${ARG_LIBNAME}_INCLUDE" "${EXT_INCLUDE_DIR}" PARENT_SCOPE)
            #set("${ARG_LIBNAME}_LIBS" "${${ARG_PACKAGE_NAME_UPPER}_LIBRARIES}" "${ARG_EXTRAFLAGS}" PARENT_SCOPE)
            if ("${ARG_EXTRAFLAGS}" STREQUAL "")
                set("${ARG_LIBNAME}_LIBS" ${${ARG_PACKAGE_NAME_UPPER}_LIBRARIES} PARENT_SCOPE)
            else ()
                convert_link_string("${${ARG_PACKAGE_NAME_UPPER}_LIBRARIES}")
                set("${ARG_LIBNAME}_LIBS" ${CONVERTED_LIB_STRING} ${ARG_EXTRAFLAGS} PARENT_SCOPE)
            endif()
            message(STATUS "Found ${ARG_PACKAGE_NAME}: ${${ARG_PACKAGE_NAME_UPPER}_LIBRARIES}")
            return()
        else ()
          #message("${ARG_LIBNAME_ORIG} NOT FOUND AS PACKAGE")
          find_libraries(EXT_LIBRARY NAMES ${ARG_LIBS})
          find_path(EXT_INCLUDE_DIR NAMES ${ARG_INCLUDES})
        endif()
    endif()

    #message(FATAL_ERROR "EXT_LIBRARY: ${EXT_LIBRARY} EXT_INCLUDE_DIR: ${EXT_INCLUDE_DIR}")
    FIND_PACKAGE_HANDLE_STANDARD_ARGS("${ARG_LIBNAME}"
        REQUIRED_VARS EXT_LIBRARY EXT_INCLUDE_DIR)
    unset(FINAL_LIBS)
    if (${${ARG_LIBNAME}_FOUND})
        set("${ARG_LIBNAME}_INCLUDE" "${EXT_INCLUDE_DIR}" PARENT_SCOPE)
        if ("${ARG_EXTRAFLAGS}" STREQUAL "")
            set("${ARG_LIBNAME}_LIBS" ${EXT_LIBRARY} PARENT_SCOPE)
        else ()
            convert_link_string("${EXT_LIBRARY}")
            set("${ARG_LIBNAME}_LIBS" ${CONVERTED_LIB_STRING} ${ARG_EXTRAFLAGS} PARENT_SCOPE)
        endif()
        #set("${ARG_LIBNAME}_LIBS" ${LINKER_FLAG_PREFIX} ${CONVERTED_LIB_STRING} ${LINKER_FLAG_SUFFIX} PARENT_SCOPE)
        #set("${ARG_LIBNAME}_LIBS" ${CONVERTED_LIB_STRING} ${ARG_EXTRAFLAGS} PARENT_SCOPE)
        #set("${ARG_LIBNAME}_LIBS" "${EXT_LIBRARY}" "${ARG_EXTRAFLAGS}" PARENT_SCOPE)
    endif()
    set("${ARG_LIBNAME}_FOUND" ${${ARG_LIBNAME}_FOUND} PARENT_SCOPE)
    if (NOT ${${ARG_LIBNAME}_FOUND})
        message(STATUS "Could NOT find ${ARG_PACKAGE_NAME}")
    endif()

endfunction()
