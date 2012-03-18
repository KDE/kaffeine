###############################################################################
# CMake module to search for the vlc libraries.
#
# WARNING: This module is experimental work in progress.
#
# This module defines:
#  VLC_INCLUDE_DIRS        = include dirs to be used when using the vlc library.
#  VLC_LIBRARY_DIRS        = directories where the libraries are located.
#  VLC_LIBRARY             = full path to the vlc library.
#  VLC_CORE_LIBRARY        = full path to the vlccore library.
#  VLC_VERSION_STRING      = the vlc version found
#       VLC_VERSION_MAJOR
#       VLC_VERSION_MINOR
#       VLC_VERSION_PATCH
#       VLC_VERSION_EXTRA
#  VLC_FOUND               = true if vlc was found.
#
# This module respects:
#  LIB_SUFFIX         = (64|32|"") Specifies the suffix for the lib directory
#
# Copyright (c) 2011 Michael Jansen <info@michael-jansen.biz>
#
# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.
#
###############################################################################

#
### Global Configuration Section
#
SET(_VLC_REQUIRED_VARS VLC_INCLUDE_DIR VLC_LIBRARY)
## FIXME
# SET(_VLC_REQUIRED_VARS VLC_INCLUDE_DIR VLC_LIBRARY VLC_VERSION_MAJOR VLC_VERSION_MINOR)

#
### VLC uses pkgconfig.
#
find_package(PkgConfig)
if(PKG_CONFIG_FOUND)
    pkg_check_modules(PC_VLC QUIET libvlc)
endif(PKG_CONFIG_FOUND)

#
### Look for the include files.
#
find_path(
    VLC_INCLUDE_DIR
    NAMES vlc/vlc.h
    HINTS
        ${PC_VLC_INCLUDEDIR}
        ${PC_VLC_INCLUDE_DIRS} # Unused for vlc but anyway
    DOC "VLC include directory"
    )
mark_as_advanced(VLC_INCLUDE_DIR)
set(VLC_INCLUDE_DIRS ${VLC_INCLUDE_DIR})

#
### Look for the libraries (vlc and vlcsore)
#
find_library(
    VLC_LIBRARY
    NAMES vlc
    HINTS
        ${PC_VLC_LIBDIR}
        ${PC_VLC_LIBRARY_DIRS} # Unused for vlc but anyway
    PATH_SUFFIXES lib${LIB_SUFFIX}
    )
get_filename_component(_VLC_LIBRARY_DIR ${VLC_LIBRARY} PATH)
mark_as_advanced(VLC_LIBRARY )

find_library(
    VLC_CORE_LIBRARY
    NAMES vlccore
    HINTS
        ${PC_VLC_LIBDIR}
        ${PC_VLC_LIBRARY_DIRS}
    PATH_SUFFIXES lib${LIB_SUFFIX}
    )
get_filename_component(_VLC_CORE_LIBRARY_DIR ${VLC_CORE_LIBRARY} PATH)
mark_as_advanced(VLC_CORE_LIBRARY )

set(VLC_LIBRARY_DIRS _VLC_CORE_LIBRARY_DIR _VLC_LIBRARY_DIR)
list(REMOVE_DUPLICATES VLC_LIBRARY_DIRS)
mark_as_advanced(VLC_LIBRARY_DIRS)

#
### Now parse the version
#
if(VLC_INCLUDE_DIR)
    if(EXISTS "${VLC_INCLUDE_DIR}/vlc/libvlc_version.h" )
        file( STRINGS "${VLC_INCLUDE_DIR}/vlc/libvlc_version.h" VLC_INFO_H REGEX "^# *define LIBVLC_VERSION_.*\\([0-9]+\\).*$")
        string(REGEX REPLACE ".*LIBVLC_VERSION_MAJOR +\\(([0-9]+)\\).*" "\\1"    VLC_VERSION_MAJOR "${VLC_INFO_H}")
        string(REGEX REPLACE ".*LIBVLC_VERSION_MINOR +\\(([0-9]+)\\).*" "\\1"    VLC_VERSION_MINOR "${VLC_INFO_H}")
        string(REGEX REPLACE ".*LIBVLC_VERSION_REVISION +\\(([0-9]+)\\).*" "\\1" VLC_VERSION_PATCH "${VLC_INFO_H}")
        string(REGEX REPLACE ".*LIBVLC_VERSION_EXTRA +\\(([0-9]+)\\).*" "\\1" VLC_VERSION_EXTRA "${VLC_INFO_H}")
        set(VLC_VERSION_STRING "${VLC_VERSION_MAJOR}.${VLC_VERSION_MINOR}.${VLC_VERSION_PATCH}.${VLC_VERSION_EXTRA}")
        mark_as_advanced(
            VLC_VERSION_MAJOR
            VLC_VERSION_MINOR
            VLC_VERSION_PATCH
            VLC_VERSION_EXTRA
            VLC_VERSION_STRING)
    else()
        message(FATAL_ERROR "Could not find vlc/vlc_version.h")
    endif()
endif()

#
### Check if everything was found and if the version is sufficient.
#
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(
    VLC
    REQUIRED_VARS ${_VLC_REQUIRED_VARS}
    VERSION_VAR VLC_VERSION_STRING
    )

