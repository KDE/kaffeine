project(kaffeine)

# search packages used by KDE
find_package(KDE4 REQUIRED)
include(KDE4Defaults)

add_definitions(${QT_DEFINITIONS} ${KDE4_DEFINITIONS})
include_directories(${CMAKE_SOURCE_DIR} ${CMAKE_BINARY_DIR} ${KDE4_INCLUDES})

add_subdirectory(deviceactions)
add_subdirectory(src)

if(BUILD_TOOLS)
  add_subdirectory(tools)
endif(BUILD_TOOLS)
