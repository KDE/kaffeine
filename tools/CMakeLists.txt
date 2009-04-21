add_executable(convertscanfiles convertscanfiles.cpp)
target_link_libraries(convertscanfiles ${QT_QTCORE_LIBRARY})

add_executable(updatedvbsi updatedvbsi.cpp)
target_link_libraries(updatedvbsi ${QT_QTXML_LIBRARY})
