project(kaffeine)

cmake_minimum_required(VERSION 3.4)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/modules")

find_package(KDE4 REQUIRED)
find_package(X11 REQUIRED)
find_package(VLC 1.2 REQUIRED)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_BINARY_DIR} ${KDE4_INCLUDES} ${VLC_INCLUDE_DIRS} ${LIBDVBV5_INCLUDE_DIR}
                    ${X11_Xscreensaver_INCLUDE_PATH})
add_definitions(${KDE4_DEFINITIONS})

if(STRICT_BUILD)
  add_definitions(-Werror -Weverything -Wno-packed -Wno-padded -Wno-sign-conversion -Wno-unreachable-code
                  -Wno-weak-vtables -ferror-limit=5
                  -isystem /usr/include/KDE
                  -isystem /usr/include/qt4/QtCore
                  -isystem /usr/include/qt4/QtGui
                  -isystem /usr/include/qt4/QtNetwork
                  -DQT_NO_CAST_FROM_ASCII -DQT_NO_CAST_FROM_BYTEARRAY -DQT_NO_CAST_TO_ASCII
                  -DQT_NO_URL_CAST_FROM_STRING -DQT_STRICT_ITERATORS)
endif(STRICT_BUILD)

check_include_file(${CMAKE_CURRENT_SOURCE_DIR}/include/frontend.h HAVE_DVB)

if(NOT HAVE_DVB)
  MESSAGE(STATUS "Disabled features:\nDVB  Digital television support for Linux\n")
  set(HAVE_DVB 0)
endif(NOT HAVE_DVB)

if(HAVE_DVB)
  macro_optional_find_package(LibV4L2)
  if(NOT LIBDVBV5_FOUND)
    set(HAVE_LIBDVBV5 0)
    MESSAGE(STATUS "Disabled features:\nDVB  Digital television support for Linux, as libdvbv5 was not found.\n")
  endif(NOT LIBDVBV5_FOUND)
endif(HAVE_DVB)

add_subdirectory(deviceactions)
add_subdirectory(dtvdaemon)
add_subdirectory(icons)
add_subdirectory(profiles)
add_subdirectory(src)
macro_optional_add_subdirectory(po)

if(BUILD_TOOLS)
  add_subdirectory(tools)
endif(BUILD_TOOLS)
