# FindOpenGLES
# ------------
# Finds the OpenGLES2 library
#
# This will define the following variables::
#
# OPENGLES2_FOUND - system has OpenGLES
# OPENGLES2_INCLUDE_DIRS - the OpenGLES include directory
# OPENGLES2_LIBRARIES - the OpenGLES libraries

if(NOT HINT_GLES_LIBNAME)
 set(HINT_GLES_LIBNAME GLESv2)
endif()

find_package(PkgConfig)
if(PKG_CONFIG_FOUND)
    pkg_check_modules(OPENGLES2 glesv2)
endif()

if(OPENGLES2_FOUND)
    set(OPENGLES2_INCLUDE_DIR ${OPENGLES2_INCLUDE_DIRS})
    set(OPENGLES2_gl_LIBRARY ${OPENGLES2_LIBRARIES})
else()
    find_path(OPENGLES2_INCLUDE_DIR GLES2/gl2.h
        PATHS "${CMAKE_FIND_ROOT_PATH}/usr/include"
        HINTS ${HINT_GLES_INCDIR}
    )

    find_library(OPENGLES2_gl_LIBRARY
        NAMES ${HINT_GLES_LIBNAME}
        HINTS ${HINT_GLES_LIBDIR}
    )
endif()

# es4all: pkg-config 對裝在預設 include 路徑的 GLES2 會回傳空的 INCLUDE_DIRS，
# 使下方 REQUIRED_VARS 檢查 OPENGLES2_INCLUDE_DIR 失敗。補 fallback 直接找標頭/庫，
# 確保獨立編譯（VM chroot / 雲編譯）在 GLES2 裝於預設路徑時也能通過。
if(NOT OPENGLES2_INCLUDE_DIR)
    find_path(OPENGLES2_INCLUDE_DIR GLES2/gl2.h
        PATHS "${CMAKE_FIND_ROOT_PATH}/usr/include"
        HINTS ${HINT_GLES_INCDIR}
    )
endif()
if(NOT OPENGLES2_gl_LIBRARY)
    find_library(OPENGLES2_gl_LIBRARY
        NAMES ${HINT_GLES_LIBNAME}
        HINTS ${HINT_GLES_LIBDIR}
    )
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(OpenGLES2
            REQUIRED_VARS OPENGLES2_gl_LIBRARY OPENGLES2_INCLUDE_DIR)


if(OPENGLES2_FOUND)
    set(OPENGLES2_LIBRARIES ${OPENGLES2_gl_LIBRARY})
    set(OPENGLES2_INCLUDE_DIRS ${OPENGLES2_INCLUDE_DIR})
    mark_as_advanced(OPENGLES2_INCLUDE_DIR OPENGLES2_gl_LIBRARY)
endif()

