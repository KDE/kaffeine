project(kaffeine)

set(kaffeine_SRCS
    dvb/dvbchannel.cpp
    kaffeine.cpp
    main.cpp
    manager.cpp
    mediawidget.cpp)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/dvb)

kde4_automoc(${kaffeine_SRCS})

kde4_add_executable(kaffeine ${kaffeine_SRCS})
target_link_libraries(kaffeine ${KDE4_KIO_LIBS} ${KDE4_PHONONUI_LIBS})

install(TARGETS kaffeine DESTINATION ${BIN_INSTALL_DIR})
install(FILES kaffeineui.rc DESTINATION ${DATA_INSTALL_DIR}/kaffeine)
