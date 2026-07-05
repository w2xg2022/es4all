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

# es4all: pkg-config 对装在默认 include 路径的 GLES2 会返回空的 INCLUDE_DIRS，
# 使下方 REQUIRED_VARS 检查 OPENGLES2_INCLUDE_DIR 失败。补 fallback 直接找头文件/库，
# 确保独立编译（VM chroot / 云编译）在 GLES2 装于默认路径时也能通过。
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

