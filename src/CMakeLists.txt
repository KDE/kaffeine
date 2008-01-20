set(kaffeine_SRCS
    dvb/dvbchannel.cpp
    dvb/dvbdevice.cpp
    dvb/dvbtab.cpp
    engine.cpp
    engine_phonon.cpp
    kaffeine.cpp
    main.cpp
    manager.cpp
    mediawidget.cpp)

configure_file(config-kaffeine.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config-kaffeine.h)
kde4_add_executable(kaffeine4 ${kaffeine_SRCS})
target_link_libraries(kaffeine4 ${KDE4_KIO_LIBS} ${KDE4_PHONON_LIBS})

install(TARGETS kaffeine4 DESTINATION ${BIN_INSTALL_DIR})
install(FILES kaffeine4ui.rc DESTINATION ${DATA_INSTALL_DIR}/kaffeine4)
