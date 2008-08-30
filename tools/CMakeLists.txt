project(kaffeine)

# search packages used by KDE
find_package(KDE4 REQUIRED)
include(KDE4Defaults)

add_definitions(${QT_DEFINITIONS} ${KDE4_DEFINITIONS})
include_directories(${CMAKE_SOURCE_DIR} ${CMAKE_BINARY_DIR} ${KDE4_INCLUDES})

add_executable(convertscanfiles convertscanfiles.cpp)
target_link_libraries(convertscanfiles ${QT_QTCORE_LIBRARY})

add_executable(updatedvbsi updatedvbsi.cpp)
target_link_libraries(updatedvbsi ${QT_QTXML_LIBRARY})
