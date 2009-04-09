add_executable(convertscanfiles convertscanfiles.cpp)
target_link_libraries(convertscanfiles ${KDE4_KDECORE_LIBS})

add_executable(updatedvbsi updatedvbsi.cpp)
target_link_libraries(updatedvbsi ${QT_QTXML_LIBRARY})
