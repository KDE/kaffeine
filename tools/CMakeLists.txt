add_executable(convertscanfiles convertscanfiles.cpp ../src/dvb/dvbchannel.cpp)
target_link_libraries(convertscanfiles ${QT_QTCORE_LIBRARY})

add_executable(updatedvbsi updatedvbsi.cpp)
target_link_libraries(updatedvbsi ${QT_QTCORE_LIBRARY} ${QT_QTXML_LIBRARY})

add_executable(updatemimetypes updatemimetypes.cpp)
target_link_libraries(updatemimetypes ${QT_QTCORE_LIBRARY} ${KDE4_KDECORE_LIBS})
