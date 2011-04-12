project(kaffeine)

find_package(KDE4 REQUIRED)
find_package(X11 REQUIRED)
find_package(Xine REQUIRED)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_BINARY_DIR} ${KDE4_INCLUDES}
                    ${X11_INCLUDE_DIR} ${X11_Xscreensaver_INCLUDE_PATH} ${XINE_INCLUDE_DIR})
add_definitions(${KDE4_DEFINITIONS})

if(STRICT_BUILD)
  # -DQT_NO_CAST_FROM_ASCII
  add_definitions(-ansi -Wnon-virtual-dtor -Wold-style-cast -Woverloaded-virtual -pedantic -Wundef
                  -DQT_NO_CAST_FROM_BYTEARRAY -DQT_NO_CAST_TO_ASCII -DQT_NO_URL_CAST_FROM_STRING
                  -DQT_STRICT_ITERATORS)
endif(STRICT_BUILD)

check_include_files(${CMAKE_CURRENT_SOURCE_DIR}/include/frontend.h HAVE_DVB)

if(NOT HAVE_DVB)
  MESSAGE(STATUS "Disabled features:\nDVB  Digital television support for Linux\n")
  set(HAVE_DVB 0)
endif(NOT HAVE_DVB)

add_subdirectory(deviceactions)
add_subdirectory(icons)
add_subdirectory(profiles)
add_subdirectory(src)
macro_optional_add_subdirectory(po)

if(BUILD_TOOLS)
  add_subdirectory(tools)
endif(BUILD_TOOLS)
