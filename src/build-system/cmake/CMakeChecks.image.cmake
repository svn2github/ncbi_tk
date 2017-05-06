############################################################################
#
# Various image-format libraries
find_package(JPEG)
find_package(PNG)
find_package(TIFF)
find_package(GIF)
find_package(Freetype)
message(STATUS "Found Freetype: Freetype_FOUND = ${Freetype_FOUND}")
message(STATUS "Found Freetype: FREETYPE_FOUND = ${FREETYPE_FOUND}")
if (Freetype_FOUND OR FREETYPE_FOUND)
    message(STATUS "Found Freetype: ${FREETYPE_LIBRARIES}")
    find_package(FTGL)
    if (FTGL_FOUND)
        message(STATUS "Found FTGL: ${FTGL_LIBRARIES}")
    else()
        message(STATUS "Cannot find FTGL")
    endif()
endif()

#
# For backward compatibility

set(IMAGE_INCLUDE_DIR
    ${JPEG_INCLUDE_DIR}
    ${PNG_INCLUDE_DIR}
    ${TIFF_INCLUDE_DIR}
    ${GIF_INCLUDE_DIR})
set(IMAGE_LIBS
    ${JPEG_LIBRARIES}
    ${PNG_LIBRARIES}
    ${TIFF_LIBRARIES}
    ${GIF_LIBRARIES})


# FreeType, FTGL
set(FREETYPE_INCLUDE ${FREETYPE_INCLUDE_DIRS})
set(FREETYPE_LIBS    ${FREETYPE_LIBRARIES})
set(FTGL_INCLUDE     ${FTGL_INCLUDE_DIR} ${FREETYPE_INCLUDE_DIRS})
set(FTGL_LIBS        ${FTGL_LIBRARIES} ${FREETYPE_LIBRARIES})

