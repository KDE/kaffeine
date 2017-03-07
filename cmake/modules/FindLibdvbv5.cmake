# - Try to find the libdvbv5 library
# Once done this will define
#
#  Libdvbv5_FOUND - system has libdvbv5
#  Libdvbv5_INCLUDE_DIRS - the libdvbv5 include directories
#  Libdvbv5_LIBRARIES - Link these to use libdvbv5

# Copyright (c) 2016, Pino Toscano <pino@kde.org>

# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1. Redistributions of source code must retain the copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
# 3. The name of the author may not be used to endorse or promote products
#    derived from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
# IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
# OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
# IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
# INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
# NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
# THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

find_package(PkgConfig)

if (PKG_CONFIG_FOUND)
  if (Libdvbv5_FIND_VERSION)
    set(version_string ">=${Libdvbv5_FIND_VERSION}")
  endif()
  pkg_check_modules(PC_LIBDVBV5 libdvbv5${version_string})
  unset(version_string)
else()
  # assume it was found
  set(PC_LIBDVBV5_FOUND TRUE)
endif()

if (PC_LIBDVBV5_FOUND)
  find_path(Libdvbv5_INCLUDE_DIRS libdvbv5/dvb-file.h
    HINTS ${PC_LIBDVBV5_INCLUDE_DIRS}
  )
  if(EXISTS "${Libdvbv5_INCLUDE_DIRS}/libdvbv5/libdvb-version.h" )
    set(HAVE_LIBDVBV5_VERSION 1)
  else()
    set(HAVE_LIBDVBV5_VERSION 0)
  endif()

  find_library(Libdvbv5_LIBRARIES NAMES dvbv5
    HINTS ${PC_LIBDVBV5_LIBRARY_DIRS}
  )

  set(Libdvbv5_VERSION "${PC_LIBDVBV5_VERSION}")
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Libdvbv5
                                  REQUIRED_VARS Libdvbv5_LIBRARIES Libdvbv5_INCLUDE_DIRS
                                  VERSION_VAR Libdvbv5_VERSION
)

mark_as_advanced(Libdvbv5_INCLUDE_DIRS Libdvbv5_LIBRARIES)
