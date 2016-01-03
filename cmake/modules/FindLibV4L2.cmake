# cmake macro to find libdvbv5
#
# Copyright (c) 2009, Jaroslav Reznik <jreznik@redhat.com>
#
# Once done this will define:
#
#  LIBDVBV5_FOUND - System has libdvbv5
#  LIBDVBV5_INCLUDE_DIR - The libdvbv5 include directory
#  LIBDVBV5_LIBRARY - The libraries needed to use libdvbv5
#
# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.

IF (LIBDVBV5_INCLUDE_DIR AND LIBDVBV5_LIBRARY)
    # Already in cache, be silent
    SET (LIBDVBV5_FIND_QUIETLY TRUE)
ENDIF (LIBDVBV5_INCLUDE_DIR AND LIBDVBV5_LIBRARY)

FIND_PATH (LIBDVBV5_INCLUDE_DIR libdvbv5/dvb-file.h)

FIND_LIBRARY (LIBDVBV5_LIBRARY libdvbv5.so)

INCLUDE (FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS (libdvbv5 DEFAULT_MSG LIBDVBV5_INCLUDE_DIR LIBDVBV5_LIBRARY)

MARK_AS_ADVANCED(LIBDVBV5_INCLUDE_DIR LIBDVBV5_LIBRARY)
