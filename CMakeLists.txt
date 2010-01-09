project(kaffeine)

find_package(KDE4 REQUIRED)
find_package(X11 REQUIRED)
find_package(Xine REQUIRED)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_BINARY_DIR} ${KDE4_INCLUDES} ${X11_INCLUDE_DIR} ${XINE_INCLUDE_DIR})
add_definitions(${KDE4_DEFINITIONS})

add_subdirectory(deviceactions)
add_subdirectory(icons)
add_subdirectory(src)
macro_optional_add_subdirectory(po)

if(BUILD_TOOLS)
  add_subdirectory(tools)
endif(BUILD_TOOLS)
